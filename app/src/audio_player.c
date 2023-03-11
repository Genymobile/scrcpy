#include "audio_player.h"

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "util/log.h"

#define SC_AUDIO_PLAYER_NDEBUG // comment to debug

/** Downcast frame_sink to sc_audio_player */
#define DOWNCAST(SINK) container_of(SINK, struct sc_audio_player, frame_sink)

#define SC_AV_SAMPLE_FMT AV_SAMPLE_FMT_FLT
#define SC_SDL_SAMPLE_FMT AUDIO_F32

#define SC_AUDIO_OUTPUT_BUFFER_MS 5

static inline uint32_t
bytes_to_samples(struct sc_audio_player *ap, size_t bytes) {
    assert(bytes % (ap->nb_channels * ap->out_bytes_per_sample) == 0);
    return bytes / (ap->nb_channels * ap->out_bytes_per_sample);
}

static inline size_t
samples_to_bytes(struct sc_audio_player *ap, uint32_t samples) {
    return samples * ap->nb_channels * ap->out_bytes_per_sample;
}

static void SDLCALL
sc_audio_player_sdl_callback(void *userdata, uint8_t *stream, int len_int) {
    struct sc_audio_player *ap = userdata;

    // This callback is called with the lock used by SDL_AudioDeviceLock(), so
    // the bytebuf is protected

    assert(len_int > 0);
    size_t len = len_int;

#ifndef SC_AUDIO_PLAYER_NDEBUG
    LOGD("[Audio] SDL callback requests %" PRIu32 " samples",
         bytes_to_samples(ap, len));
#endif

    size_t read_avail = sc_bytebuf_read_available(&ap->buf);
    if (!ap->played) {
        uint32_t buffered_samples = bytes_to_samples(ap, read_avail);

        // Part of the buffering is handled by inserting initial silence. The
        // remaining (margin) last samples will be handled by compensation.
        uint32_t margin = 30 * ap->sample_rate / 1000; // 30ms
        if (buffered_samples + margin < ap->target_buffering) {
            LOGV("[Audio] Inserting initial buffering silence: %" PRIu32
                 " samples", bytes_to_samples(ap, len));
            // Delay playback starting to reach the target buffering. Fill the
            // whole buffer with silence (len is small compared to the
            // arbitrary margin value).
            memset(stream, 0, len);
            return;
        }
    }

    size_t read = MIN(read_avail, len);
    if (read) {
        sc_bytebuf_read(&ap->buf, stream, read);
    }

    if (read < len) {
        size_t silence_bytes = len - read;
        uint32_t silence_samples = bytes_to_samples(ap, silence_bytes);
        // Insert silence. In theory, the inserted silent samples replace the
        // missing real samples, which will arrive later, so they should be
        // dropped to keep the latency minimal. However, this would cause very
        // audible glitches, so let the clock compensation restore the target
        // latency.
        LOGD("[Audio] Buffer underflow, inserting silence: %" PRIu32 " samples",
             silence_samples);
        memset(stream + read, 0, silence_bytes);

        if (ap->received) {
            // Inserting additional samples immediately increases buffering
            ap->avg_buffering.avg += silence_samples;
        }
    }

    ap->played = true;
}

static uint8_t *
sc_audio_player_get_swr_buf(struct sc_audio_player *ap, uint32_t min_samples) {
    size_t min_buf_size = samples_to_bytes(ap, min_samples);
    if (min_buf_size > ap->swr_buf_alloc_size) {
        size_t new_size = min_buf_size + 4096;
        uint8_t *buf = realloc(ap->swr_buf, new_size);
        if (!buf) {
            LOG_OOM();
            // Could not realloc to the requested size
            return NULL;
        }
        ap->swr_buf = buf;
        ap->swr_buf_alloc_size = new_size;
    }

    return ap->swr_buf;
}

static bool
sc_audio_player_frame_sink_push(struct sc_frame_sink *sink,
                                const AVFrame *frame) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    SwrContext *swr_ctx = ap->swr_ctx;

    int64_t swr_delay = swr_get_delay(swr_ctx, ap->sample_rate);
    // No need to av_rescale_rnd(), input and output sample rates are the same.
    // Add more space (256) for clock compensation.
    int dst_nb_samples = swr_delay + frame->nb_samples + 256;

    uint8_t *swr_buf = sc_audio_player_get_swr_buf(ap, dst_nb_samples);
    if (!swr_buf) {
        return false;
    }

    int ret = swr_convert(swr_ctx, &swr_buf, dst_nb_samples,
                          (const uint8_t **) frame->data, frame->nb_samples);
    if (ret < 0) {
        LOGE("Resampling failed: %d", ret);
        return false;
    }

    // swr_convert() returns the number of samples which would have been
    // written if the buffer was big enough.
    uint32_t samples_written = MIN(ret, dst_nb_samples);
    size_t swr_buf_size = samples_to_bytes(ap, samples_written);
#ifndef SC_AUDIO_PLAYER_NDEBUG
    LOGD("[Audio] %" PRIu32 " samples written to buffer", samples_written);
#endif

    // Since this function is the only writer, the current available space is
    // at least the previous available space. In practice, it should almost
    // always be possible to write without lock.
    bool lockless_write = swr_buf_size <= ap->previous_write_avail;
    if (lockless_write) {
        sc_bytebuf_prepare_write(&ap->buf, swr_buf, swr_buf_size);
    }

    SDL_LockAudioDevice(ap->device);

    size_t read_avail = sc_bytebuf_read_available(&ap->buf);
    uint32_t buffered_samples = bytes_to_samples(ap, read_avail);

    if (lockless_write) {
        sc_bytebuf_commit_write(&ap->buf, swr_buf_size);
    } else {
        // Take care to keep full samples
        size_t align = ap->nb_channels * ap->out_bytes_per_sample;
        size_t write_avail =
            sc_bytebuf_write_available(&ap->buf) / align * align;
        if (swr_buf_size > write_avail) {
            // Entering this branch is very unlikely, the ring-buffer (bytebuf)
            // is allocated with a size sufficient to store 1 second more than
            // the target buffering. If this happens, though, we have to skip
            // old samples.
            size_t cap = sc_bytebuf_capacity(&ap->buf) / align * align;
            if (swr_buf_size > cap) {
                // Very very unlikely: a single resampled frame should never
                // exceed the ring-buffer size (or something is very wrong).
                // Ignore the first bytes in swr_buf
                swr_buf += swr_buf_size - cap;
                swr_buf_size = cap;
                // This change in samples_written will impact the
                // instant_compensation below
                samples_written -= bytes_to_samples(ap, swr_buf_size - cap);
            }

            assert(swr_buf_size >= write_avail);
            if (swr_buf_size > write_avail) {
                sc_bytebuf_skip(&ap->buf, swr_buf_size - write_avail);
                uint32_t skip_samples =
                    bytes_to_samples(ap, swr_buf_size - write_avail);
                assert(buffered_samples >= skip_samples);
                buffered_samples -= skip_samples;
                if (ap->played) {
                    // Dropping input samples instantly decreases buffering
                    ap->avg_buffering.avg -= skip_samples;
                }
            }

            // It should remain exactly the expected size to write the new
            // samples.
            assert((sc_bytebuf_write_available(&ap->buf) / align * align)
                    == swr_buf_size);
        }

        sc_bytebuf_write(&ap->buf, swr_buf, swr_buf_size);
    }

    buffered_samples += samples_written;
    assert(samples_to_bytes(ap, buffered_samples)
            == sc_bytebuf_read_available(&ap->buf));

    // Read with lock held, to be used after unlocking
    bool played = ap->played;
    if (played) {
        uint32_t max_buffered_samples = ap->target_buffering
                + 12 * SC_AUDIO_OUTPUT_BUFFER_MS * ap->sample_rate / 1000
                + ap->target_buffering / 10;
        if (buffered_samples > max_buffered_samples) {
            uint32_t skip_samples = buffered_samples - max_buffered_samples;
            size_t skip_bytes = samples_to_bytes(ap, skip_samples);
            sc_bytebuf_skip(&ap->buf, skip_bytes);
            LOGD("[Audio] Buffering threshold exceeded, skipping %" PRIu32
                 " samples", skip_samples);
        }

        // Number of samples added (or removed, if negative) for compensation
        int32_t instant_compensation =
            (int32_t) samples_written - frame->nb_samples;

        // The compensation must apply instantly, it must not be smoothed
        ap->avg_buffering.avg += instant_compensation;

        // However, the buffering level must be smoothed
        sc_average_push(&ap->avg_buffering, buffered_samples);

#ifndef SC_AUDIO_PLAYER_NDEBUG
        LOGD("[Audio] buffered_samples=%" PRIu32 " avg_buffering=%f",
             buffered_samples, sc_average_get(&ap->avg_buffering));
#endif
    } else {
        // SDL playback not started yet, do not accumulate more than
        // max_initial_buffering samples, this would cause unnecessary delay
        // (and glitches to compensate) on start.
        uint32_t max_initial_buffering = ap->target_buffering
                + 2 * SC_AUDIO_OUTPUT_BUFFER_MS * ap->sample_rate / 1000;
        if (buffered_samples > max_initial_buffering) {
            uint32_t skip_samples = buffered_samples - max_initial_buffering;
            size_t skip_bytes = samples_to_bytes(ap, skip_samples);
            sc_bytebuf_skip(&ap->buf, skip_bytes);
#ifndef SC_AUDIO_PLAYER_NDEBUG
            LOGD("[Audio] Playback not started, skipping %" PRIu32 " samples",
                 skip_samples);
#endif
        }
    }

    ap->previous_write_avail = sc_bytebuf_write_available(&ap->buf);
    ap->received = true;

    SDL_UnlockAudioDevice(ap->device);

    if (played) {
        ap->samples_since_resync += samples_written;
        if (ap->samples_since_resync >= ap->sample_rate) {
            // Recompute compensation every second
            ap->samples_since_resync = 0;

            float avg = sc_average_get(&ap->avg_buffering);
            int diff = ap->target_buffering - avg;
            if (diff < 0 && buffered_samples < ap->target_buffering) {
                // Do not accelerate if the instant buffering level is below
                // the average, this would increase underflow
                diff = 0;
            }
            // Compensate the diff over 4 seconds (but will be recomputed after
            // 1 second)
            int distance = 4 * ap->sample_rate;
            // Limit compensation rate to 2%
            int abs_max_diff = distance / 50;
            diff = CLAMP(diff, -abs_max_diff, abs_max_diff);
            LOGV("[Audio] Buffering: target=%" PRIu32 " avg=%f cur=%" PRIu32
                 " compensation=%d", ap->target_buffering, avg,
                 buffered_samples, diff);
            int ret = swr_set_compensation(swr_ctx, diff, distance);
            if (ret < 0) {
                LOGW("Resampling compensation failed: %d", ret);
                // not fatal
            }
        }
    }

    return true;
}

static bool
sc_audio_player_frame_sink_open(struct sc_frame_sink *sink,
                                const AVCodecContext *ctx) {
    struct sc_audio_player *ap = DOWNCAST(sink);
#ifdef SCRCPY_LAVU_HAS_CHLAYOUT
    assert(ctx->ch_layout.nb_channels > 0);
    unsigned nb_channels = ctx->ch_layout.nb_channels;
#else
    int tmp = av_get_channel_layout_nb_channels(ctx->channel_layout);
    assert(tmp > 0);
    unsigned nb_channels = tmp;
#endif

    SDL_AudioSpec desired = {
        .freq = ctx->sample_rate,
        .format = SC_SDL_SAMPLE_FMT,
        .channels = nb_channels,
        .samples = SC_AUDIO_OUTPUT_BUFFER_MS * ctx->sample_rate / 1000,
        .callback = sc_audio_player_sdl_callback,
        .userdata = ap,
    };
    SDL_AudioSpec obtained;

    ap->device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (!ap->device) {
        LOGE("Could not open audio device: %s", SDL_GetError());
        return false;
    }

    SwrContext *swr_ctx = swr_alloc();
    if (!swr_ctx) {
        LOG_OOM();
        goto error_close_audio_device;
    }
    ap->swr_ctx = swr_ctx;

    assert(ctx->sample_rate > 0);
    assert(!av_sample_fmt_is_planar(SC_AV_SAMPLE_FMT));
    int out_bytes_per_sample = av_get_bytes_per_sample(SC_AV_SAMPLE_FMT);
    assert(out_bytes_per_sample > 0);

#ifdef SCRCPY_LAVU_HAS_CHLAYOUT
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &ctx->ch_layout, 0);
    av_opt_set_chlayout(swr_ctx, "out_chlayout", &ctx->ch_layout, 0);
#else
    av_opt_set_channel_layout(swr_ctx, "in_channel_layout",
                              ctx->channel_layout, 0);
    av_opt_set_channel_layout(swr_ctx, "out_channel_layout",
                              ctx->channel_layout, 0);
#endif

    av_opt_set_int(swr_ctx, "in_sample_rate", ctx->sample_rate, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", ctx->sample_rate, 0);

    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", SC_AV_SAMPLE_FMT, 0);

    int ret = swr_init(swr_ctx);
    if (ret) {
        LOGE("Failed to initialize the resampling context");
        goto error_free_swr_ctx;
    }

    ap->sample_rate = ctx->sample_rate;
    ap->nb_channels = nb_channels;
    ap->out_bytes_per_sample = out_bytes_per_sample;

    ap->target_buffering = ap->target_buffering_delay * ap->sample_rate
                                                      / SC_TICK_FREQ;

    // Use a ring-buffer of the target buffering size plus 1 second between the
    // producer and the consumer. It's too big on purpose, to guarantee that
    // the producer and the consumer will be able to access it in parallel
    // without locking.
    size_t bytebuf_samples = ap->target_buffering + ap->sample_rate;
    size_t bytebuf_size = samples_to_bytes(ap, bytebuf_samples);

    bool ok = sc_bytebuf_init(&ap->buf, bytebuf_size);
    if (!ok) {
        goto error_free_swr_ctx;
    }

    size_t initial_swr_buf_size = samples_to_bytes(ap, 4096);
    ap->swr_buf = malloc(initial_swr_buf_size);
    if (!ap->swr_buf) {
        LOG_OOM();
        goto error_destroy_bytebuf;
    }
    ap->swr_buf_alloc_size = initial_swr_buf_size;

    ap->previous_write_avail = sc_bytebuf_write_available(&ap->buf);

    // Samples are produced and consumed by blocks, so the buffering must be
    // smoothed to get a relatively stable value.
    sc_average_init(&ap->avg_buffering, 32);
    ap->samples_since_resync = 0;

    ap->received = false;
    ap->played = false;

    // The thread calling open() is the thread calling push(), which fills the
    // audio buffer consumed by the SDL audio thread.
    ok = sc_thread_set_priority(SC_THREAD_PRIORITY_TIME_CRITICAL);
    if (!ok) {
        ok = sc_thread_set_priority(SC_THREAD_PRIORITY_HIGH);
        (void) ok; // We don't care if it worked, at least we tried
    }

    SDL_PauseAudioDevice(ap->device, 0);

    return true;

error_destroy_bytebuf:
    sc_bytebuf_destroy(&ap->buf);
error_free_swr_ctx:
    swr_free(&ap->swr_ctx);
error_close_audio_device:
    SDL_CloseAudioDevice(ap->device);

    return false;
}

static void
sc_audio_player_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    assert(ap->device);
    SDL_PauseAudioDevice(ap->device, 1);
    SDL_CloseAudioDevice(ap->device);

    free(ap->swr_buf);
    sc_bytebuf_destroy(&ap->buf);
    swr_free(&ap->swr_ctx);
}

void
sc_audio_player_init(struct sc_audio_player *ap, sc_tick target_buffering) {
    ap->target_buffering_delay = target_buffering;

    static const struct sc_frame_sink_ops ops = {
        .open = sc_audio_player_frame_sink_open,
        .close = sc_audio_player_frame_sink_close,
        .push = sc_audio_player_frame_sink_push,
    };

    ap->frame_sink.ops = &ops;
}
