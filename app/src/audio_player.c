#include "audio_player.h"

#include <libavutil/opt.h>

#include "util/log.h"

//#define SC_AUDIO_PLAYER_NDEBUG // comment to debug

/** Downcast frame_sink to sc_audio_player */
#define DOWNCAST(SINK) container_of(SINK, struct sc_audio_player, frame_sink)

#define SC_AV_SAMPLE_FMT AV_SAMPLE_FMT_FLT
#define SC_SDL_SAMPLE_FMT AUDIO_F32

#define SC_AUDIO_OUTPUT_BUFFER_SAMPLES 480 // 10ms at 48000Hz

// The target number of buffered samples between the producer and the consumer.
// This value is directly use for compensation.
#define SC_TARGET_BUFFERED_SAMPLES (3 * SC_AUDIO_OUTPUT_BUFFER_SAMPLES)

// Use a ring-buffer of 1 second (at 48000Hz) between the producer and the
// consumer. It too big, but it guarantees that the producer and the consumer
// will be able to access it in parallel without locking.
#define SC_BYTEBUF_SIZE_IN_SAMPLES 48000

static inline size_t
bytes_to_samples(struct sc_audio_player *ap, size_t bytes) {
    assert(bytes % (ap->nb_channels * ap->out_bytes_per_sample) == 0);
    return bytes / (ap->nb_channels * ap->out_bytes_per_sample);
}

static inline size_t
samples_to_bytes(struct sc_audio_player *ap, size_t samples) {
    return samples * ap->nb_channels * ap->out_bytes_per_sample;
}

void
sc_audio_player_sdl_callback(void *userdata, uint8_t *stream, int len_int) {
    struct sc_audio_player *ap = userdata;

    // This callback is called with the lock used by SDL_AudioDeviceLock(), so
    // the bytebuf is protected

    assert(len_int > 0);
    size_t len = len_int;

#ifndef SC_AUDIO_PLAYER_NDEBUG
    LOGD("[Audio] SDL callback requests %" SC_PRIsizet " samples",
         bytes_to_samples(ap, len));
#endif

    size_t read_avail = sc_bytebuf_read_available(&ap->buf);
    size_t read = MIN(read_avail, len);
    if (read) {
        sc_bytebuf_read(&ap->buf, stream, read);
    }

    if (read < len) {
        // Insert silence
#ifndef SC_AUDIO_PLAYER_NDEBUG
        LOGD("[Audio] Buffer underflow, inserting silence: %" SC_PRIsizet
             " samples", bytes_to_samples(ap, len - read));
#endif
        memset(stream + read, 0, len - read);
        // If the first frame has not been received yet, it's not an underflow
        if (ap->received) {
            ap->underflow += bytes_to_samples(ap, len - read);
        }
    }

    ap->last_consumed = sc_tick_now();
}

static uint8_t *
sc_audio_player_get_swr_buf(struct sc_audio_player *ap, size_t min_samples) {
    size_t min_buf_size = samples_to_bytes(ap, min_samples);
    if (min_buf_size < ap->swr_buf_alloc_size) {
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
sc_audio_player_frame_sink_open(struct sc_frame_sink *sink,
                                const AVCodecContext *ctx) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    SDL_AudioSpec desired = {
        .freq = ctx->sample_rate,
        .format = SC_SDL_SAMPLE_FMT,
        .channels = ctx->ch_layout.nb_channels,
        .samples = SC_AUDIO_OUTPUT_BUFFER_SAMPLES,
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
    assert(ctx->ch_layout.nb_channels > 0);
    assert(!av_sample_fmt_is_planar(SC_AV_SAMPLE_FMT));
    int out_bytes_per_sample = av_get_bytes_per_sample(SC_AV_SAMPLE_FMT);
    assert(out_bytes_per_sample > 0);

    av_opt_set_chlayout(swr_ctx, "in_chlayout", &ctx->ch_layout, 0);
    av_opt_set_chlayout(swr_ctx, "out_chlayout", &ctx->ch_layout, 0);

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
    ap->nb_channels = ctx->ch_layout.nb_channels;
    ap->out_bytes_per_sample = out_bytes_per_sample;

    size_t bytebuf_size = samples_to_bytes(ap, SC_BYTEBUF_SIZE_IN_SAMPLES);

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

    sc_average_init(&ap->avg_buffering, 8);
    ap->samples_since_resync = 0;

    ap->last_consumed = 0;
    ap->underflow = 0;
    ap->received = 0;

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

static bool
sc_audio_player_frame_sink_push(struct sc_frame_sink *sink,
                                const AVFrame *frame) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    SwrContext *swr_ctx = ap->swr_ctx;

    int64_t delay = swr_get_delay(swr_ctx, ap->sample_rate);
    // No need to av_rescale_rnd(), input and output sample rates are the same
    // Add more space (256) for clock compensation
    int dst_nb_samples = delay + frame->nb_samples + 256;

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
    size_t samples_written = MIN(ret, dst_nb_samples);
    size_t swr_buf_size = samples_to_bytes(ap, samples_written);
#ifndef SC_AUDIO_PLAYER_NDEBUG
    LOGD("[Audio] %" SC_PRIsizet " samples written to buffer", samples_written);
#endif

    // Since this function is the only writer, the current available space is
    // at least the previous available space. In practice, it should almost
    // always be possible to write without lock.
    bool lockless_write = swr_buf_size <= ap->previous_write_avail;
    if (lockless_write) {
        sc_bytebuf_prepare_write(&ap->buf, swr_buf, swr_buf_size);
    }

    SDL_LockAudioDevice(ap->device);

    // The consumer requests audio samples blocks (e.g. 480 samples).
    // Convert the duration since the last consumption into samples.
    size_t extrapolated = 0;
    if (ap->last_consumed) {
        sc_tick now = sc_tick_now();
        assert(now >= ap->last_consumed);
        extrapolated = (now - ap->last_consumed) * ap->sample_rate
                                                 / SC_TICK_FREQ;
    }

    size_t read_avail = sc_bytebuf_read_available(&ap->buf);

    // The consumer may not increase underflow value if there are still samples
    // available
    assert(read_avail == 0 || ap->underflow == 0);

    size_t buffered_samples = bytes_to_samples(ap, read_avail);
    // Underflow caused silence samples in excess (so it adds buffering).
    // Extrapolated samples must be considered consumed for smoothing (so it
    // removes buffering).
    float buffering = (float) buffered_samples + ap->underflow - extrapolated;
    sc_average_push(&ap->avg_buffering, buffering);

#ifndef SC_AUDIO_PLAYER_NDEBUG
    LOGD("[Audio] buffered_samples=%" SC_PRIsizet
                " underflow=%" SC_PRIsizet
                " extrapolated=%" SC_PRIsizet
                " buffering=%f avg_buffering=%f",
         buffered_samples, ap->underflow, extrapolated, buffering,
         sc_average_get(&ap->avg_buffering));
#endif

    if (lockless_write) {
        sc_bytebuf_commit_write(&ap->buf, swr_buf_size);
    } else {
        // Take care to keep full samples
        size_t align = ap->nb_channels * ap->out_bytes_per_sample;
        size_t write_avail =
            sc_bytebuf_write_available(&ap->buf) / align * align;
        if (swr_buf_size > write_avail) {
            // Skip old samples
            size_t cap = sc_bytebuf_capacity(&ap->buf) / align * align;
            if (swr_buf_size > cap) {
                // Ignore the first bytes in swr_buf
                swr_buf += swr_buf_size - cap;
                swr_buf_size = cap;
            }
            assert(swr_buf_size > write_avail);
            if (swr_buf_size - write_avail > 0) {
                sc_bytebuf_skip(&ap->buf, swr_buf_size - write_avail);
            }
        }
        sc_bytebuf_write(&ap->buf, swr_buf, swr_buf_size);
    }

    // On buffer underflow, typically because a packet is late, silence is
    // inserted. In that case, the late samples must be ignored when they
    // arrive, otherwise they will delay playback.
    //
    // As an improvement, instead of naively skipping the silence duration, we
    // can absorb it if it helps clock compensation.
    if (ap->underflow) {
        size_t avg = sc_average_get(&ap->avg_buffering);
        if (avg > SC_TARGET_BUFFERED_SAMPLES) {
            size_t diff = SC_TARGET_BUFFERED_SAMPLES - avg;
            if (ap->underflow > diff) {
                // Partially absorb underflow for clock compensation (only keep
                // the diff with the target buffering level).
                ap->underflow -= diff;
            } else {
                // Totally absorb underflow for clock compensation
                ap->underflow = 0;
            }

            size_t skip_samples = MIN(ap->underflow, buffered_samples);
            if (skip_samples) {
                size_t skip_bytes = samples_to_bytes(ap, skip_samples);
                sc_bytebuf_skip(&ap->buf, skip_bytes);
                read_avail -= skip_bytes;
#ifndef SC_AUDIO_PLAYER_NDEBUG
                LOGD("[Audio] Skipping %" SC_PRIsizet " samples", skip_samples);
#endif
            }
        } else {
            // Totally absorb underflow for clock compensation
            ap->underflow = 0;
        }
    }

    ap->previous_write_avail = sc_bytebuf_write_available(&ap->buf);
    ap->received = true;

    SDL_UnlockAudioDevice(ap->device);

    ap->samples_since_resync += samples_written;
    if (ap->samples_since_resync >= ap->sample_rate) {
        // Resync every second
        ap->samples_since_resync = 0;

        float avg = sc_average_get(&ap->avg_buffering);
        int diff = SC_TARGET_BUFFERED_SAMPLES - avg;
#ifndef SC_AUDIO_PLAYER_NDEBUG
        LOGD("[Audio] Average buffering=%f, compensation %d", avg, diff);
#endif
        // Compensate the diff over 3 seconds (but will be recomputed after
        // 1 second)
        int ret = swr_set_compensation(swr_ctx, diff, 3 * ap->sample_rate);
        if (ret < 0) {
            LOGW("Resampling compensation failed: %d", ret);
            // not fatal
        }
    }

    return true;
}

void
sc_audio_player_init(struct sc_audio_player *ap) {
    static const struct sc_frame_sink_ops ops = {
        .open = sc_audio_player_frame_sink_open,
        .close = sc_audio_player_frame_sink_close,
        .push = sc_audio_player_frame_sink_push,
    };

    ap->frame_sink.ops = &ops;
}
