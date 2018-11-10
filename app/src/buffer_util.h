#ifndef BUFFER_UTIL_H
#define BUFFER_UTIL_H

#include <SDL2/SDL_stdinc.h>

static inline void buffer_write16be(Uint8 *buf, Uint16 value) {
    buf[0] = value >> 8;
    buf[1] = value;
}

static inline void buffer_write32be(Uint8 *buf, Uint32 value) {
    buf[0] = value >> 24;
    buf[1] = value >> 16;
    buf[2] = value >> 8;
    buf[3] = value;
}

#endif
