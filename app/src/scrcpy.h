#ifndef SCRCPY_H
#define SCRCPY_H

#include <SDL2/SDL_stdinc.h>

struct scrcpy_options {
    const char *serial;
    const char *crop;
    const char *record_filename;
    Uint16 port;
    Uint16 max_size;
    Uint32 bit_rate;
    SDL_bool show_touches;
    SDL_bool fullscreen;
};

SDL_bool scrcpy(const struct scrcpy_options *options);

#endif
