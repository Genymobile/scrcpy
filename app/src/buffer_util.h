#ifndef BUFFER_UTIL_H
#define BUFFER_UTIL_H

#include <stdbool.h>
#include <stdint.h>

static inline void
buffer_write16be(uint8_t *buf, uint16_t value) {
    buf[0] = value >> 8;
    buf[1] = value;
}

static inline void
buffer_write32be(uint8_t *buf, uint32_t value) {
    buf[0] = value >> 24;
    buf[1] = value >> 16;
    buf[2] = value >> 8;
    buf[3] = value;
}

static inline uint32_t
buffer_read32be(uint8_t *buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static inline
uint64_t buffer_read64be(uint8_t *buf) {
    uint32_t msb = buffer_read32be(buf);
    uint32_t lsb = buffer_read32be(&buf[4]);
    return ((uint64_t) msb << 32) | lsb;
}

#endif
