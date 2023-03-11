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
    size_t head; // writter cursor
    size_t tail; // reader cursor
    // empty: tail == head
    // full: ((tail + 1) % alloc_size) == head
};

bool
sc_bytebuf_init(struct sc_bytebuf *buf, size_t alloc_size);

/**
 * Copy from the bytebuf to a user-provided array
 *
 * The caller must check that len <= sc_bytebuf_read_available() (it is an
 * error to attempt to read more bytes than available).
 *
 * This function is guaranteed not to write to buf->head.
 */
void
sc_bytebuf_read(struct sc_bytebuf *buf, uint8_t *to, size_t len);

/**
 * Drop len bytes from the buffer
 *
 * The caller must check that len <= sc_bytebuf_read_available() (it is an
 * error to attempt to skip more bytes than available).
 *
 * This function is guaranteed not to write to buf->head.
 *
 * It is equivalent to call sc_bytebuf_read() to some array and discard the
 * array (but this function is more efficient since there is no copy).
 */
void
sc_bytebuf_skip(struct sc_bytebuf *buf, size_t len);

/**
 * Copy the user-provided array to the bytebuf
 *
 * The caller must check that len <= sc_bytebuf_write_available() (it is an
 * error to write more bytes than the remaining available space).
 *
 * This function is guaranteed not to write to buf->tail.
 */
void
sc_bytebuf_write(struct sc_bytebuf *buf, const uint8_t *from, size_t len);

/**
 * Copy the user-provided array to the bytebuf, but do not advance the cursor
 *
 * The caller must check that len <= sc_bytebuf_write_available() (it is an
 * error to write more bytes than the remaining available space).
 *
 * After this function is called, the write must be committed with
 * sc_bytebuf_commit_write().
 *
 * The purpose of this mechanism is to acquire a lock only to commit the write,
 * but not to perform the actual copy.
 *
 * This function is guaranteed not to access buf->tail.
 */
void
sc_bytebuf_prepare_write(struct sc_bytebuf *buf, const uint8_t *from,
                         size_t len);

/**
 * Commit a prepared write
 */
void
sc_bytebuf_commit_write(struct sc_bytebuf *buf, size_t len);

/**
 * Return the number of bytes which can be read
 *
 * It is an error to read more bytes than available.
 */
static inline size_t
sc_bytebuf_can_read(struct sc_bytebuf *buf) {
    return (buf->alloc_size + buf->head - buf->tail) % buf->alloc_size;
}

/**
 * Return the number of bytes which can be written
 *
 * It is an error to write more bytes than available.
 */
static inline size_t
sc_bytebuf_can_write(struct sc_bytebuf *buf) {
    return (buf->alloc_size + buf->tail - buf->head - 1) % buf->alloc_size;
}

/**
 * Return the actual capacity of the buffer (can_read() + can_write())
 */
static inline size_t
sc_bytebuf_capacity(struct sc_bytebuf *buf) {
    return buf->alloc_size - 1;
}

void
sc_bytebuf_destroy(struct sc_bytebuf *buf);

#endif
