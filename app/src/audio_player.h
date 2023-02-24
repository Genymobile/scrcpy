#ifndef SC_AUDIO_PLAYER_H
#define SC_AUDIO_PLAYER_H

#include "common.h"

#include <stdbool.h>
#include "trait/frame_sink.h"
#include <util/bytebuf.h>
#include <util/thread.h>

#include <libavformat/avformat.h>
#include <SDL2/SDL.h>

struct sc_audio_player {
    struct sc_frame_sink frame_sink;

    SDL_AudioDeviceID device;

    // protected by SDL_AudioDeviceLock()
    struct sc_bytebuf buf;
    // Number of bytes which could be written without locking
    size_t safe_empty_buffer;

    const struct sc_audio_player_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_audio_player_callbacks {
    void (*on_ended)(struct sc_audio_player *ap, bool success, void *userdata);
};

bool
sc_audio_player_init(struct sc_audio_player *ap,
                     const struct sc_audio_player_callbacks *cbs,
                     void *cbs_userdata);

void
sc_audio_player_destroy(struct sc_audio_player *ap);

#endif
