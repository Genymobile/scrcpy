#ifndef SC_STRBUF_H
#define SC_STRBUF_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct sc_strbuf {
    char *s;
    size_t len;
    size_t cap;
};

/**
 * Initialize the string buffer
 *
 * `buf->s` must be manually freed by the caller.
 */
bool
sc_strbuf_init(struct sc_strbuf *buf, size_t init_cap);

/**
 * Append a string
 *
 * Append `len` characters from `s` to the buffer.
 */
bool
sc_strbuf_append(struct sc_strbuf *buf, const char *s, size_t len);

/**
 * Append a char
 *
 * Append a single character to the buffer.
 */
bool
sc_strbuf_append_char(struct sc_strbuf *buf, const char c);

/**
 * Append a char `n` times
 *
 * Append the same characters `n` times to the buffer.
 */
bool
sc_strbuf_append_n(struct sc_strbuf *buf, const char c, size_t n);

/**
 * Append a NUL-terminated string
 */
static inline bool
sc_strbuf_append_str(struct sc_strbuf *buf, const char *s) {
    return sc_strbuf_append(buf, s, strlen(s));
}

/**
 * Append a static string
 *
 * Append a string whose size is known at compile time (for
 * example a string literal).
 */
#define sc_strbuf_append_staticstr(BUF, S) \
    sc_strbuf_append(BUF, S, sizeof(S) - 1)

/**
 * Shrink the buffer capacity to its current length
 *
 * This resizes `buf->s` to fit the content.
 */
void
sc_strbuf_shrink(struct sc_strbuf *buf);

#endif
