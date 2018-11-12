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

static inline Uint32 buffer_read32be(Uint8 *buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static inline Uint64 buffer_read64be(Uint8 *buf) {
    Uint32 msb = buffer_read32be(buf);
    Uint32 lsb = buffer_read32be(&buf[4]);
    return ((Uint64) msb << 32) | lsb;
}

#endif
