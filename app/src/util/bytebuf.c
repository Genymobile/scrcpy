#include "bytebuf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util/log.h"

bool
sc_bytebuf_init(struct sc_bytebuf *buf, size_t alloc_size) {
    assert(alloc_size);
    buf->data = malloc(alloc_size);
    if (!buf->data) {
        LOG_OOM();
        return false;
    }

    buf->alloc_size = alloc_size;
    buf->head = 0;
    buf->tail = 0;

    return true;
}

void
sc_bytebuf_destroy(struct sc_bytebuf *buf) {
    free(buf->data);
}

void
sc_bytebuf_read(struct sc_bytebuf *buf, uint8_t *to, size_t len) {
    assert(len);
    assert(len <= sc_bytebuf_can_read(buf));
    assert(buf->tail != buf->head); // the buffer could not be empty

    size_t right_limit = buf->tail < buf->head ? buf->head : buf->alloc_size;
    size_t right_len = right_limit - buf->tail;
    if (len < right_len) {
        right_len = len;
    }
    memcpy(to, buf->data + buf->tail, right_len);

    if (len > right_len) {
        memcpy(to + right_len, buf->data, len - right_len);
    }

    buf->tail = (buf->tail + len) % buf->alloc_size;
}

void
sc_bytebuf_skip(struct sc_bytebuf *buf, size_t len) {
    assert(len);
    assert(len <= sc_bytebuf_can_read(buf));
    assert(buf->tail != buf->head); // the buffer could not be empty

    buf->tail = (buf->tail + len) % buf->alloc_size;
}

static inline void
sc_bytebuf_write_step0(struct sc_bytebuf *buf, const uint8_t *from,
                       size_t len) {
    size_t right_len = buf->alloc_size - buf->head;
    if (len < right_len) {
        right_len = len;
    }
    memcpy(buf->data + buf->head, from, right_len);

    if (len > right_len) {
        memcpy(buf->data, from + right_len, len - right_len);
    }
}

static inline void
sc_bytebuf_write_step1(struct sc_bytebuf *buf, size_t len) {
    buf->head = (buf->head + len) % buf->alloc_size;
}

void
sc_bytebuf_write(struct sc_bytebuf *buf, const uint8_t *from, size_t len) {
    assert(len);
    assert(len <= sc_bytebuf_can_write(buf));

    sc_bytebuf_write_step0(buf, from, len);
    sc_bytebuf_write_step1(buf, len);
}

void
sc_bytebuf_prepare_write(struct sc_bytebuf *buf, const uint8_t *from,
                         size_t len) {
    // *This function MUST NOT access buf->tail (even in assert()).*
    // The purpose of this function is to allow a reader and a writer to access
    // different parts of the buffer in parallel simultaneously. It is intended
    // to be called without lock (only sc_bytebuf_commit_write() is intended to
    // be called with lock held).

    assert(len < buf->alloc_size - 1);
    sc_bytebuf_write_step0(buf, from, len);
}

void
sc_bytebuf_commit_write(struct sc_bytebuf *buf, size_t len) {
    assert(len <= sc_bytebuf_can_write(buf));
    sc_bytebuf_write_step1(buf, len);
}
