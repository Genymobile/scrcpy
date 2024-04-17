#ifndef SC_AUDIO_PLAYER_H
#define SC_AUDIO_PLAYER_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>

#include "trait/frame_sink.h"
#include "util/audiobuf.h"
#include "util/average.h"
#include "util/thread.h"
#include "util/tick.h"

struct sc_audio_player {
    struct sc_frame_sink frame_sink;

    SDL_AudioDeviceID device;

    // The target buffering between the producer and the consumer. This value
    // is directly use for compensation.
    // Since audio capture and/or encoding on the device typically produce
    // blocks of 960 samples (20ms) or 1024 samples (~21.3ms), this target
    // value should be higher.
    sc_tick target_buffering_delay;
    uint32_t target_buffering; // in samples

    // SDL audio output buffer size.
    sc_tick output_buffer_duration;
    uint16_t output_buffer;

    // Audio buffer to communicate between the receiver and the SDL audio
    // callback
    struct sc_audiobuf buf;

    // Resampler (only used from the receiver thread)
    struct SwrContext *swr_ctx;

    // The sample rate is the same for input and output
    unsigned sample_rate;
    // The number of channels is the same for input and output
    unsigned nb_channels;
    // The number of bytes per sample for a single channel
    size_t out_bytes_per_sample;

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

    // Current applied compensation value (only used by the receiver thread)
    int compensation;

    // Set to true the first time a sample is received
    atomic_bool received;

    // Set to true the first time the SDL callback is called
    atomic_bool played;

    const struct sc_audio_player_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_audio_player_callbacks {
    void (*on_ended)(struct sc_audio_player *ap, bool success, void *userdata);
};

void
sc_audio_player_init(struct sc_audio_player *ap, sc_tick target_buffering,
                     sc_tick audio_output_buffer);

#endif
