#include "audio_regulator.h"

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "util/log.h"

//#define SC_AUDIO_REGULATOR_DEBUG // uncomment to debug

/**
 * Real-time audio regulator with configurable latency
 *
 * As input, the regulator regularly receives AVFrames of decoded audio samples.
 * As output, the audio player regularly requests audio samples to be played.
 * In the middle, an audio buffer stores the samples produced but not consumed
 * yet.
 *
 * The goal of the regulator is to feed the audio player with a latency as low
 * as possible while avoiding buffer underrun (i.e. not being able to provide
 * samples when requested).
 *
 * To achieve this, it attempts to maintain the average buffering (the number
 * of samples present in the buffer) around a target value. If this target
 * buffering is too low, then buffer underrun will occur frequently. If it is
 * too high, then latency will become unacceptable. This target value is
 * configured using the scrcpy option --audio-buffer.
 *
 * The regulator cannot adjust the sample input rate (it receives samples
 * produced in real-time) or the sample output rate (it must provide samples as
 * requested by the audio player). Therefore, it may only apply compensation by
 * resampling (converting _m_ input samples to _n_ output samples).
 *
 * The compensation itself is applied by libswresample (FFmpeg). It is
 * configured using swr_set_compensation(). An important work for the regulator
 * is to estimate the compensation value regularly and apply it.
 *
 * The estimated buffering level is the result of averaging the "natural"
 * buffering (samples are produced and consumed by blocks, so it must be
 * smoothed), and making instant adjustments resulting of its own actions
 * (explicit compensation and silence insertion on underflow), which are not
 * smoothed.
 *
 * Buffer underflow events can occur when packets arrive too late. In that case,
 * the regulator inserts silence. Once the packets finally arrive (late), one
 * strategy could be to drop the samples that were replaced by silence, in
 * order to keep a minimal latency. However, dropping samples in case of buffer
 * underflow is inadvisable, as it would temporarily increase the underflow
 * even more and cause very noticeable audio glitches.
 *
 * Therefore, the regulator doesn't drop any sample on underflow. The
 * compensation mechanism will absorb the delay introduced by the inserted
 * silence.
 */

#define TO_BYTES(SAMPLES) sc_audiobuf_to_bytes(&ar->buf, (SAMPLES))
#define TO_SAMPLES(BYTES) sc_audiobuf_to_samples(&ar->buf, (BYTES))

void
sc_audio_regulator_pull(struct sc_audio_regulator *ar, uint8_t *out,
                        uint32_t out_samples) {
#ifdef SC_AUDIO_REGULATOR_DEBUG
    LOGD("[Audio] Audio regulator pulls %" PRIu32 " samples", out_samples);
#endif

    // A lock is necessary in the rare case where the producer needs to drop
    // samples already pushed (when the buffer is full)
    sc_mutex_lock(&ar->mutex);

    bool played = atomic_load_explicit(&ar->played, memory_order_relaxed);
    if (!played) {
        uint32_t buffered_samples = sc_audiobuf_can_read(&ar->buf);
        // Wait until the buffer is filled up to at least target_buffering
        // before playing
        if (buffered_samples < ar->target_buffering) {
            LOGV("[Audio] Inserting initial buffering silence: %" PRIu32
                 " samples", out_samples);
            // Delay playback starting to reach the target buffering. Fill the
            // whole buffer with silence (len is small compared to the
            // arbitrary margin value).
            memset(out, 0, out_samples * ar->sample_size);
            sc_mutex_unlock(&ar->mutex);
            return;
        }
    }

    uint32_t read = sc_audiobuf_read(&ar->buf, out, out_samples);

    sc_mutex_unlock(&ar->mutex);

    if (read < out_samples) {
        uint32_t silence = out_samples - read;
        // Insert silence. In theory, the inserted silent samples replace the
        // missing real samples, which will arrive later, so they should be
        // dropped to keep the latency minimal. However, this would cause very
        // audible glitches, so let the clock compensation restore the target
        // latency.
        LOGD("[Audio] Buffer underflow, inserting silence: %" PRIu32 " samples",
             silence);
        memset(out + TO_BYTES(read), 0, TO_BYTES(silence));

        bool received = atomic_load_explicit(&ar->received,
                                             memory_order_relaxed);
        if (received) {
            // Inserting additional samples immediately increases buffering
            atomic_fetch_add_explicit(&ar->underflow, silence,
                                      memory_order_relaxed);
        }
    }

    atomic_store_explicit(&ar->played, true, memory_order_relaxed);
}

static uint8_t *
sc_audio_regulator_get_swr_buf(struct sc_audio_regulator *ar,
                               uint32_t min_samples) {
    size_t min_buf_size = TO_BYTES(min_samples);
    if (min_buf_size > ar->swr_buf_alloc_size) {
        size_t new_size = min_buf_size + 4096;
        uint8_t *buf = realloc(ar->swr_buf, new_size);
        if (!buf) {
            LOG_OOM();
            // Could not realloc to the requested size
            return NULL;
        }
        ar->swr_buf = buf;
        ar->swr_buf_alloc_size = new_size;
    }

    return ar->swr_buf;
}

bool
sc_audio_regulator_push(struct sc_audio_regulator *ar, const AVFrame *frame) {
    SwrContext *swr_ctx = ar->swr_ctx;

    int64_t swr_delay = swr_get_delay(swr_ctx, ar->sample_rate);
    // No need to av_rescale_rnd(), input and output sample rates are the same.
    // Add more space (256) for clock compensation.
    int dst_nb_samples = swr_delay + frame->nb_samples + 256;

    uint8_t *swr_buf = sc_audio_regulator_get_swr_buf(ar, dst_nb_samples);
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
    uint32_t samples = MIN(ret, dst_nb_samples);
#ifdef SC_AUDIO_REGULATOR_DEBUG
    LOGD("[Audio] %" PRIu32 " samples written to buffer", samples);
#endif

    uint32_t cap = sc_audiobuf_capacity(&ar->buf);
    if (samples > cap) {
        // Very very unlikely: a single resampled frame should never
        // exceed the audio buffer size (or something is very wrong).
        // Ignore the first bytes in swr_buf to avoid memory corruption anyway.
        swr_buf += TO_BYTES(samples - cap);
        samples = cap;
    }

    uint32_t skipped_samples = 0;

    uint32_t written = sc_audiobuf_write(&ar->buf, swr_buf, samples);
    if (written < samples) {
        uint32_t remaining = samples - written;

        // All samples that could be written without locking have been written,
        // now we need to lock to drop/consume old samples
        sc_mutex_lock(&ar->mutex);

        // Retry with the lock
        written += sc_audiobuf_write(&ar->buf,
                                     swr_buf + TO_BYTES(written),
                                     remaining);
        if (written < samples) {
            remaining = samples - written;
            // Still insufficient, drop old samples to make space
            skipped_samples = sc_audiobuf_read(&ar->buf, NULL, remaining);
            assert(skipped_samples == remaining);
        }

        sc_mutex_unlock(&ar->mutex);

        if (written < samples) {
            // Now there is enough space
            uint32_t w = sc_audiobuf_write(&ar->buf,
                                           swr_buf + TO_BYTES(written),
                                           remaining);
            assert(w == remaining);
            (void) w;
        }
    }

    uint32_t underflow = 0;
    uint32_t max_buffered_samples;
    bool played = atomic_load_explicit(&ar->played, memory_order_relaxed);
    if (played) {
        underflow = atomic_exchange_explicit(&ar->underflow, 0,
                                             memory_order_relaxed);

        max_buffered_samples = ar->target_buffering * 11 / 10
                             + 60 * ar->sample_rate / 1000 /* 60 ms */;
    } else {
        // Playback not started yet, do not accumulate more than
        // max_initial_buffering samples, this would cause unnecessary delay
        // (and glitches to compensate) on start.
        max_buffered_samples = ar->target_buffering
                             + 10 * ar->sample_rate / 1000 /* 10 ms */;
    }

    uint32_t can_read = sc_audiobuf_can_read(&ar->buf);
    if (can_read > max_buffered_samples) {
        uint32_t skip_samples = 0;

        sc_mutex_lock(&ar->mutex);
        can_read = sc_audiobuf_can_read(&ar->buf);
        if (can_read > max_buffered_samples) {
            skip_samples = can_read - max_buffered_samples;
            uint32_t r = sc_audiobuf_read(&ar->buf, NULL, skip_samples);
            assert(r == skip_samples);
            (void) r;
            skipped_samples += skip_samples;
        }
        sc_mutex_unlock(&ar->mutex);

        if (skip_samples) {
            if (played) {
                LOGD("[Audio] Buffering threshold exceeded, skipping %" PRIu32
                     " samples", skip_samples);
#ifdef SC_AUDIO_REGULATOR_DEBUG
            } else {
                LOGD("[Audio] Playback not started, skipping %" PRIu32
                     " samples", skip_samples);
#endif
            }
        }
    }

    atomic_store_explicit(&ar->received, true, memory_order_relaxed);
    if (!played) {
        // Nothing more to do
        return true;
    }

    // Number of samples added (or removed, if negative) for compensation
    int32_t instant_compensation = (int32_t) written - frame->nb_samples;
    // Inserting silence instantly increases buffering
    int32_t inserted_silence = (int32_t) underflow;
    // Dropping input samples instantly decreases buffering
    int32_t dropped = (int32_t) skipped_samples;

    // The compensation must apply instantly, it must not be smoothed
    ar->avg_buffering.avg += instant_compensation + inserted_silence - dropped;
    if (ar->avg_buffering.avg < 0) {
        // Since dropping samples instantly reduces buffering, the difference
        // is applied immediately to the average value, assuming that the delay
        // between the producer and the consumer will be caught up.
        //
        // However, when this assumption is not valid, the average buffering
        // may decrease indefinitely. Prevent it to become negative to limit
        // the consequences.
        ar->avg_buffering.avg = 0;
    }

    // However, the buffering level must be smoothed
    sc_average_push(&ar->avg_buffering, can_read);

#ifdef SC_AUDIO_REGULATOR_DEBUG
    LOGD("[Audio] can_read=%" PRIu32 " avg_buffering=%f",
         can_read, sc_average_get(&ar->avg_buffering));
#endif

    ar->samples_since_resync += written;
    if (ar->samples_since_resync >= ar->sample_rate) {
        // Recompute compensation every second
        ar->samples_since_resync = 0;

        float avg = sc_average_get(&ar->avg_buffering);
        int diff = ar->target_buffering - avg;

        // Enable compensation when the difference exceeds +/- 4ms.
        // Disable compensation when the difference is lower than +/- 1ms.
        int threshold = ar->compensation_active
                      ? ar->sample_rate     / 1000  /* 1ms */
                      : ar->sample_rate * 4 / 1000; /* 4ms */

        if (abs(diff) < threshold) {
            // Do not compensate for small values, the error is just noise
            diff = 0;
        } else if (diff < 0 && can_read < ar->target_buffering) {
            // Do not accelerate if the instant buffering level is below the
            // target, this would increase underflow
            diff = 0;
        }
        // Compensate the diff over 4 seconds (but will be recomputed after 1
        // second)
        int distance = 4 * ar->sample_rate;
        // Limit compensation rate to 2%
        int abs_max_diff = distance / 50;
        diff = CLAMP(diff, -abs_max_diff, abs_max_diff);
        LOGV("[Audio] Buffering: target=%" PRIu32 " avg=%f cur=%" PRIu32
             " compensation=%d", ar->target_buffering, avg, can_read, diff);

        int ret = swr_set_compensation(swr_ctx, diff, distance);
        if (ret < 0) {
            LOGW("Resampling compensation failed: %d", ret);
            // not fatal
        } else {
            ar->compensation_active = diff != 0;
        }
    }

    return true;
}

bool
sc_audio_regulator_init(struct sc_audio_regulator *ar, size_t sample_size,
                        const AVCodecContext *ctx, uint32_t target_buffering) {
    SwrContext *swr_ctx = swr_alloc();
    if (!swr_ctx) {
        LOG_OOM();
        return false;
    }
    ar->swr_ctx = swr_ctx;

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

    bool ok = sc_mutex_init(&ar->mutex);
    if (!ok) {
        goto error_free_swr_ctx;
    }

    ar->target_buffering = target_buffering;
    ar->sample_size = sample_size;
    ar->sample_rate = ctx->sample_rate;

    // Use a ring-buffer of the target buffering size plus 1 second between the
    // producer and the consumer. It's too big on purpose, to guarantee that
    // the producer and the consumer will be able to access it in parallel
    // without locking.
    uint32_t audiobuf_samples = target_buffering + ar->sample_rate;

    ok = sc_audiobuf_init(&ar->buf, sample_size, audiobuf_samples);
    if (!ok) {
        goto error_destroy_mutex;
    }

    size_t initial_swr_buf_size = TO_BYTES(4096);
    ar->swr_buf = malloc(initial_swr_buf_size);
    if (!ar->swr_buf) {
        LOG_OOM();
        goto error_destroy_audiobuf;
    }
    ar->swr_buf_alloc_size = initial_swr_buf_size;

    // Samples are produced and consumed by blocks, so the buffering must be
    // smoothed to get a relatively stable value.
    sc_average_init(&ar->avg_buffering, 128);
    ar->samples_since_resync = 0;

    ar->received = false;
    atomic_init(&ar->played, false);
    atomic_init(&ar->received, false);
    atomic_init(&ar->underflow, 0);
    ar->compensation_active = false;

    return true;

error_destroy_audiobuf:
    sc_audiobuf_destroy(&ar->buf);
error_destroy_mutex:
    sc_mutex_destroy(&ar->mutex);
error_free_swr_ctx:
    swr_free(&ar->swr_ctx);

    return false;
}

void
sc_audio_regulator_destroy(struct sc_audio_regulator *ar) {
    free(ar->swr_buf);
    sc_audiobuf_destroy(&ar->buf);
    sc_mutex_destroy(&ar->mutex);
    swr_free(&ar->swr_ctx);
}
