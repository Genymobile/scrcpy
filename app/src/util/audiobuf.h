#ifndef SC_AUDIOBUF_H
#define SC_AUDIOBUF_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "util/bytebuf.h"

/**
 * Wrapper around bytebuf to read and write samples
 *
 * Each sample takes sample_size bytes.
 */
struct sc_audiobuf {
    struct sc_bytebuf buf;
    size_t sample_size;
};

static inline uint32_t
sc_audiobuf_to_samples(struct sc_audiobuf *buf, size_t bytes) {
    assert(bytes % buf->sample_size == 0);
    return bytes / buf->sample_size;
}

static inline size_t
sc_audiobuf_to_bytes(struct sc_audiobuf *buf, uint32_t samples) {
    return samples * buf->sample_size;
}

static inline bool
sc_audiobuf_init(struct sc_audiobuf *buf, size_t sample_size,
                 uint32_t capacity) {
    buf->sample_size = sample_size;
    return sc_bytebuf_init(&buf->buf, capacity * sample_size + 1);
}

static inline void
sc_audiobuf_read(struct sc_audiobuf *buf, uint8_t *to, uint32_t samples) {
    size_t bytes = sc_audiobuf_to_bytes(buf, samples);
    sc_bytebuf_read(&buf->buf, to, bytes);
}

static inline void
sc_audiobuf_skip(struct sc_audiobuf *buf, uint32_t samples) {
    size_t bytes = sc_audiobuf_to_bytes(buf, samples);
    sc_bytebuf_skip(&buf->buf, bytes);
}

static inline void
sc_audiobuf_write(struct sc_audiobuf *buf, const uint8_t *from,
                  uint32_t samples) {
    size_t bytes = sc_audiobuf_to_bytes(buf, samples);
    sc_bytebuf_write(&buf->buf, from, bytes);
}

static inline void
sc_audiobuf_prepare_write(struct sc_audiobuf *buf, const uint8_t *from,
                          uint32_t samples) {
    size_t bytes = sc_audiobuf_to_bytes(buf, samples);
    sc_bytebuf_prepare_write(&buf->buf, from, bytes);
}

static inline void
sc_audiobuf_commit_write(struct sc_audiobuf *buf, uint32_t samples) {
    size_t bytes = sc_audiobuf_to_bytes(buf, samples);
    sc_bytebuf_commit_write(&buf->buf, bytes);
}

static inline uint32_t
sc_audiobuf_can_read(struct sc_audiobuf *buf) {
    size_t bytes = sc_bytebuf_can_read(&buf->buf);
    return sc_audiobuf_to_samples(buf, bytes);
}

static inline uint32_t
sc_audiobuf_can_write(struct sc_audiobuf *buf) {
    size_t bytes = sc_bytebuf_can_write(&buf->buf);
    return sc_audiobuf_to_samples(buf, bytes);
}

static inline uint32_t
sc_audiobuf_capacity(struct sc_audiobuf *buf) {
    size_t bytes = sc_bytebuf_capacity(&buf->buf);
    return sc_audiobuf_to_samples(buf, bytes);
}

static inline void
sc_audiobuf_destroy(struct sc_audiobuf *buf) {
    sc_bytebuf_destroy(&buf->buf);
}

#endif
