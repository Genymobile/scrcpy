#ifndef SC_AUDIO_PLAYER_H
#define SC_AUDIO_PLAYER_H

#include "common.h"

#include <stdbool.h>
#include "trait/frame_sink.h"
#include <util/average.h>
#include <util/bytebuf.h>
#include <util/thread.h>

#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>

struct sc_audio_player {
    struct sc_frame_sink frame_sink;

    SDL_AudioDeviceID device;

    // protected by SDL_AudioDeviceLock()
    struct sc_bytebuf buf;
    // Number of bytes which could be written without locking
    size_t safe_empty_buffer;

    struct SwrContext *swr_ctx;

    // The sample rate is the same for input and output
    unsigned sample_rate;
    // The number of channels is the same for input and output
    unsigned nb_channels;

    unsigned out_bytes_per_sample;

    // Target buffer for resampling
    uint8_t *swr_buf;
    size_t swr_buf_alloc_size;

    // Number of buffered samples (may be negative on underflow)
    struct sc_average avg_buffered_samples;
    unsigned samples_since_resync;

    const struct sc_audio_player_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_audio_player_callbacks {
    void (*on_ended)(struct sc_audio_player *ap, bool success, void *userdata);
};

void
sc_audio_player_init(struct sc_audio_player *ap);

#endif
