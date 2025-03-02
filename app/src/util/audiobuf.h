#ifndef SC_AUDIOBUF_H
#define SC_AUDIOBUF_H

#include "common.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Wrapper around bytebuf to read and write samples
 *
 * Each sample takes sample_size bytes.
 */
struct sc_audiobuf {
    uint8_t *data;
    uint32_t alloc_size; // in samples
    size_t sample_size;

    atomic_uint_least32_t head; // writer cursor, in samples
    atomic_uint_least32_t tail; // reader cursor, in samples
    // empty: tail == head
    // full: ((tail + 1) % alloc_size) == head
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

bool
sc_audiobuf_init(struct sc_audiobuf *buf, size_t sample_size,
                 uint32_t capacity);

void
sc_audiobuf_destroy(struct sc_audiobuf *buf);

uint32_t
sc_audiobuf_read(struct sc_audiobuf *buf, void *to, uint32_t samples_count);

uint32_t
sc_audiobuf_write(struct sc_audiobuf *buf, const void *from,
                  uint32_t samples_count);

uint32_t
sc_audiobuf_write_silence(struct sc_audiobuf *buf, uint32_t samples);

static inline uint32_t
sc_audiobuf_capacity(struct sc_audiobuf *buf) {
    assert(buf->alloc_size);
    return buf->alloc_size - 1;
}

static inline uint32_t
sc_audiobuf_can_read(struct sc_audiobuf *buf) {
    uint32_t head = atomic_load_explicit(&buf->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&buf->tail, memory_order_acquire);
    return (buf->alloc_size + head - tail) % buf->alloc_size;
}

#endif
