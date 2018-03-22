#ifndef SCRCPY_H
#define SCRCPY_H

#include <SDL2/SDL_stdinc.h>
#include "config.h"

struct scrcpy_options {
    const char *serial;
    Uint16 port;
    Uint16 max_size;
    Uint32 bit_rate;
    SDL_bool show_touches;
#ifdef AUDIO_SUPPORT
    SDL_bool forward_audio;
#endif
};

SDL_bool scrcpy(const struct scrcpy_options *options);

#endif
