#include "strbuf.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

bool
sc_strbuf_init(struct sc_strbuf *buf, size_t init_cap) {
    buf->s = malloc(init_cap + 1); // +1 for '\0'
    if (!buf->s) {
        LOG_OOM();
        return false;
    }

    buf->len = 0;
    buf->cap = init_cap;
    return true;
}

static bool
sc_strbuf_reserve(struct sc_strbuf *buf, size_t len) {
    if (buf->len + len > buf->cap) {
        size_t new_cap = buf->cap * 3 / 2 + len;
        char *s = realloc(buf->s, new_cap + 1); // +1 for '\0'
        if (!s) {
            // Leave the old buf->s
            LOG_OOM();
            return false;
        }
        buf->s = s;
        buf->cap = new_cap;
    }
    return true;
}

bool
sc_strbuf_append(struct sc_strbuf *buf, const char *s, size_t len) {
    assert(s);
    assert(*s);
    assert(strlen(s) >= len);
    if (!sc_strbuf_reserve(buf, len)) {
        return false;
    }

    memcpy(&buf->s[buf->len], s, len);
    buf->len += len;
    buf->s[buf->len] = '\0';

    return true;
}

bool
sc_strbuf_append_char(struct sc_strbuf *buf, const char c) {
    if (!sc_strbuf_reserve(buf, 1)) {
        return false;
    }

    buf->s[buf->len] = c;
    buf->len ++;
    buf->s[buf->len] = '\0';

    return true;
}

bool
sc_strbuf_append_n(struct sc_strbuf *buf, const char c, size_t n) {
    if (!sc_strbuf_reserve(buf, n)) {
        return false;
    }

    memset(&buf->s[buf->len], c, n);
    buf->len += n;
    buf->s[buf->len] = '\0';

    return true;
}

void
sc_strbuf_shrink(struct sc_strbuf *buf) {
    assert(buf->len <= buf->cap);
    if (buf->len != buf->cap) {
        char *s = realloc(buf->s, buf->len + 1); // +1 for '\0'
        assert(s); // decreasing the size may not fail
        buf->s = s;
        buf->cap = buf->len;
    }
}
