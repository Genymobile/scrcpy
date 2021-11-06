#ifndef SC_STRBUF_H
#define SC_STRBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct sc_strbuf {
    char *s;
    size_t len;
    size_t cap;
};

// buf->s must be manually freed by the caller
bool
sc_strbuf_init(struct sc_strbuf *buf, size_t init_cap);

bool
sc_strbuf_append(struct sc_strbuf *buf, const char *s, size_t len);

bool
sc_strbuf_append_char(struct sc_strbuf *buf, const char c);

bool
sc_strbuf_append_n(struct sc_strbuf *buf, const char c, size_t n);

static inline bool
sc_strbuf_append_str(struct sc_strbuf *buf, const char *s) {
    return sc_strbuf_append(buf, s, strlen(s));
}

// Append static string (i.e. the string size is known at compile time, for
// example a string literal)
#define sc_strbuf_append_staticstr(BUF, S) \
    sc_strbuf_append(BUF, S, sizeof(S) - 1)

#endif
