#ifndef SC_BYTEBUF_H
#define SC_BYTEBUF_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

struct sc_bytebuf {
    uint8_t *data;
    // The actual capacity is (allocated - 1) so that head == tail is
    // non-ambiguous
    size_t alloc_size;
    size_t head;
    size_t tail;
    // empty: tail == head
    // full: (tail + 1) % allocated == head
};

bool
sc_bytebuf_init(struct sc_bytebuf *buf, size_t alloc_size);

/**
 * Copy from the bytebuf to a user-provided array
 *
 * The caller must check that len <= buf->len (it is an error to attempt to read
 * more bytes than available).
 */
void
sc_bytebuf_read(struct sc_bytebuf *buf, uint8_t *to, size_t len);

/**
 * Drop len bytes from the buffer
 *
 * The caller must check that len <= buf->len (it is an error to attempt to skip
 * more bytes than available).
 *
 * It is equivalent to call sc_bytebuf_read() to some array and discard the
 * array (but more efficient since there is no copy).
 */
void
sc_bytebuf_skip(struct sc_bytebuf *buf, size_t len);

/**
 * Copy the user-provided array to the bytebuf
 *
 * The length of the input array is not restricted:
 * if len >= sc_bytebuf_write_remaining(buf), then the excessive input bytes
 * will overwrite the oldest bytes in the buffer.
 */
void
sc_bytebuf_write(struct sc_bytebuf *buf, const uint8_t *from, size_t len);

/**
 * Return the number of bytes which can be read
 *
 * It is an error to read more bytes than available.
 */
static inline size_t
sc_bytebuf_read_remaining(struct sc_bytebuf *buf) {
    return (buf->alloc_size + buf->head - buf->tail) % buf->alloc_size;
}

/**
 * Return the number of bytes which can be written without overwriting
 *
 * It is not an error to write more bytes than the available space, but this
 * would overwrite the oldest bytes in the buffer.
 */
static inline size_t
sc_bytebuf_write_remaining(struct sc_bytebuf *buf) {
    return (buf->alloc_size + buf->tail - buf->head - 1) % buf->alloc_size;
}

void
sc_bytebuf_destroy(struct sc_bytebuf *buf);

#endif
