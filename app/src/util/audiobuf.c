#include "audiobuf.h"

#include <stdlib.h>
#include <string.h>
#include <util/log.h>
#include <util/memory.h>

bool
sc_audiobuf_init(struct sc_audiobuf *buf, size_t sample_size,
                 uint32_t capacity) {
    assert(sample_size);
    assert(capacity);

    // The actual capacity is (alloc_size - 1) so that head == tail is
    // non-ambiguous
    buf->alloc_size = capacity + 1;
    buf->data = sc_allocarray(buf->alloc_size, sample_size);
    if (!buf->data) {
        LOG_OOM();
        return false;
    }

    buf->sample_size = sample_size;
    atomic_init(&buf->head, 0);
    atomic_init(&buf->tail, 0);

    return true;
}

void
sc_audiobuf_destroy(struct sc_audiobuf *buf) {
    free(buf->data);
}

uint32_t
sc_audiobuf_read(struct sc_audiobuf *buf, void *to_, uint32_t samples_count) {
    assert(samples_count);

    uint8_t *to = to_;
    assert(to);

    // The tail cursor may be updated by the writer thread to drop samples
    uint32_t tail = atomic_load_explicit(&buf->tail, memory_order_acquire);

    // The head cursor is updated after the data is written to the array
    uint32_t head = atomic_load_explicit(&buf->head, memory_order_acquire);

    uint32_t can_read = (buf->alloc_size + head - tail) % buf->alloc_size;
    if (samples_count > can_read) {
        samples_count = can_read;
    }

    uint32_t right_count = buf->alloc_size - tail;
    if (right_count > samples_count) {
        right_count = samples_count;
    }
    memcpy(to,
           buf->data + (tail * buf->sample_size),
           right_count * buf->sample_size);

    if (samples_count > right_count) {
        uint32_t left_count = samples_count - right_count;
        memcpy(to + (right_count * buf->sample_size),
               buf->data,
               left_count * buf->sample_size);
    }

    uint32_t new_tail = (tail + samples_count) % buf->alloc_size;
    atomic_store_explicit(&buf->tail, new_tail, memory_order_release);

    return samples_count;
}

uint32_t
sc_audiobuf_write(struct sc_audiobuf *buf, const void *from_,
                  uint32_t samples_count) {
    const uint8_t *from = from_;

    // Only the writer thread can write head, so memory_order_relaxed is
    // sufficient
    uint32_t head = atomic_load_explicit(&buf->head, memory_order_relaxed);

    // The tail cursor is updated after the data is consumed by the reader
    uint32_t tail = atomic_load_explicit(&buf->tail, memory_order_acquire);

    uint32_t can_write = (buf->alloc_size + tail - head - 1) % buf->alloc_size;
    if (samples_count > can_write) {
        samples_count = can_write;
    }

    uint32_t right_count = buf->alloc_size - head;
    if (right_count > samples_count) {
        right_count = samples_count;
    }
    memcpy(buf->data + (head * buf->sample_size),
           from,
           right_count * buf->sample_size);

    if (samples_count > right_count) {
        uint32_t left_count = samples_count - right_count;
        memcpy(buf->data,
               from + (right_count * buf->sample_size),
               left_count * buf->sample_size);
    }

    uint32_t new_head = (head + samples_count) % buf->alloc_size;
    atomic_store_explicit(&buf->head, new_head, memory_order_release);

    return samples_count;
}

uint32_t
sc_audiobuf_truncate(struct sc_audiobuf *buf, uint32_t samples_limit) {
    for (;;) {
        // Only the writer thread can write head, so memory_order_relaxed is
        // sufficient
        uint32_t head = atomic_load_explicit(&buf->head, memory_order_relaxed);

        // The tail cursor is updated after the data is consumed by the reader
        uint32_t tail = atomic_load_explicit(&buf->tail, memory_order_acquire);

        uint32_t can_read = (buf->alloc_size + head - tail) % buf->alloc_size;
        if (can_read <= samples_limit) {
            // Nothing to truncate
            return 0;
        }

        uint32_t skip = can_read - samples_limit;
        uint32_t new_tail = (tail + skip) % buf->alloc_size;
        if (atomic_compare_exchange_weak(&buf->tail, &tail, new_tail)) {
            return skip;
        }
    }
}
