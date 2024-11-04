#ifndef SC_AUDIO_REGULATOR_H
#define SC_AUDIO_REGULATOR_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include "util/audiobuf.h"
#include "util/average.h"
#include "util/thread.h"

#define SC_AV_SAMPLE_FMT AV_SAMPLE_FMT_FLT

struct sc_audio_regulator {
    sc_mutex mutex;

    // Target buffering between the producer and the consumer (in samples)
    uint32_t target_buffering;

    // Audio buffer to communicate between the receiver and the player
    struct sc_audiobuf buf;

    // Resampler (only used from the receiver thread)
    struct SwrContext *swr_ctx;

    // The sample rate is the same for input and output
    uint32_t sample_rate;
    // The number of bytes per sample (for all channels)
    size_t sample_size;

    // Target buffer for resampling (only used by the receiver thread)
    uint8_t *swr_buf;
    size_t swr_buf_alloc_size;

    // Number of buffered samples (may be negative on underflow) (only used by
    // the receiver thread)
    struct sc_average avg_buffering;
    // Count the number of samples to trigger a compensation update regularly
    // (only used by the receiver thread)
    uint32_t samples_since_resync;

    // Number of silence samples inserted since the last received packet
    atomic_uint_least32_t underflow;

    // Non-zero compensation applied (only used by the receiver thread)
    bool compensation_active;

    // Set to true the first time a sample is received
    atomic_bool received;

    // Set to true the first time samples are pulled by the player
    atomic_bool played;
};

bool
sc_audio_regulator_init(struct sc_audio_regulator *ar, size_t sample_size,
                        const AVCodecContext *ctx, uint32_t target_buffering);

void
sc_audio_regulator_destroy(struct sc_audio_regulator *ar);

bool
sc_audio_regulator_push(struct sc_audio_regulator *ar, const AVFrame *frame);

void
sc_audio_regulator_pull(struct sc_audio_regulator *ar, uint8_t *out,
                        uint32_t samples);

#endif
