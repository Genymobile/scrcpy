#include "str.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <windows.h>
# include <tchar.h>
#endif

#include "log.h"
#include "strbuf.h"

size_t
sc_strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; ++i)
        dest[i] = src[i];
    if (n)
        dest[i] = '\0';
    return src[i] == '\0' ? i : n;
}

size_t
sc_str_join(char *dst, const char *const tokens[], char sep, size_t n) {
    const char *const *remaining = tokens;
    const char *token = *remaining++;
    size_t i = 0;
    while (token) {
        if (i) {
            dst[i++] = sep;
            if (i == n)
                goto truncated;
        }
        size_t w = sc_strncpy(dst + i, token, n - i);
        if (w >= n - i)
            goto truncated;
        i += w;
        token = *remaining++;
    }
    return i;

truncated:
    dst[n - 1] = '\0';
    return n;
}

char *
sc_str_quote(const char *src) {
    size_t len = strlen(src);
    char *quoted = malloc(len + 3);
    if (!quoted) {
        LOG_OOM();
        return NULL;
    }
    memcpy(&quoted[1], src, len);
    quoted[0] = '"';
    quoted[len + 1] = '"';
    quoted[len + 2] = '\0';
    return quoted;
}

bool
sc_str_parse_integer(const char *s, long *out) {
    char *endptr;
    if (*s == '\0') {
        return false;
    }
    errno = 0;
    long value = strtol(s, &endptr, 0);
    if (errno == ERANGE) {
        return false;
    }
    if (*endptr != '\0') {
        return false;
    }

    *out = value;
    return true;
}

size_t
sc_str_parse_integers(const char *s, const char sep, size_t max_items,
                      long *out) {
    size_t count = 0;
    char *endptr;
    do {
        errno = 0;
        long value = strtol(s, &endptr, 0);
        if (errno == ERANGE) {
            return 0;
        }

        if (endptr == s || (*endptr != sep && *endptr != '\0')) {
            return 0;
        }

        out[count++] = value;
        if (*endptr == sep) {
            if (count >= max_items) {
                // max items already reached, could not accept a new item
                return 0;
            }
            // parse the next token during the next iteration
            s = endptr + 1;
        }
    } while (*endptr != '\0');

    return count;
}

bool
sc_str_parse_integer_with_suffix(const char *s, long *out) {
    char *endptr;
    if (*s == '\0') {
        return false;
    }
    errno = 0;
    long value = strtol(s, &endptr, 0);
    if (errno == ERANGE) {
        return false;
    }
    int mul = 1;
    if (*endptr != '\0') {
        if (s == endptr) {
            return false;
        }
        if ((*endptr == 'M' || *endptr == 'm') && endptr[1] == '\0') {
            mul = 1000000;
        } else if ((*endptr == 'K' || *endptr == 'k') && endptr[1] == '\0') {
            mul = 1000;
        } else {
            return false;
        }
    }

    if ((value < 0 && LONG_MIN / mul > value) ||
        (value > 0 && LONG_MAX / mul < value)) {
        return false;
    }

    *out = value * mul;
    return true;
}

bool
sc_str_list_contains(const char *list, char sep, const char *s) {
    char *p;
    do {
        p = strchr(list, sep);

        size_t token_len = p ? (size_t) (p - list) : strlen(list);
        if (!strncmp(list, s, token_len)) {
            return true;
        }

        if (p) {
            list = p + 1;
        }
    } while (p);
    return false;
}

size_t
sc_str_utf8_truncation_index(const char *utf8, size_t max_len) {
    size_t len = strlen(utf8);
    if (len <= max_len) {
        return len;
    }
    len = max_len;
    // see UTF-8 encoding <https://en.wikipedia.org/wiki/UTF-8#Description>
    while ((utf8[len] & 0x80) != 0 && (utf8[len] & 0xc0) != 0xc0) {
        // the next byte is not the start of a new UTF-8 codepoint
        // so if we would cut there, the character would be truncated
        len--;
    }
    return len;
}

#ifdef _WIN32

wchar_t *
sc_str_to_wchars(const char *utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (!len) {
        return NULL;
    }

    wchar_t *wide = malloc(len * sizeof(wchar_t));
    if (!wide) {
        LOG_OOM();
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

char *
sc_str_from_wchars(const wchar_t *ws) {
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (!len) {
        return NULL;
    }

    char *utf8 = malloc(len);
    if (!utf8) {
        LOG_OOM();
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, ws, -1, utf8, len, NULL, NULL);
    return utf8;
}

#endif

char *
sc_str_wrap_lines(const char *input, unsigned columns, unsigned indent) {
    assert(indent < columns);

    struct sc_strbuf buf;

    // The output string should not be much longer than the input string (just
    // a few '\n' added), so this initial capacity should hopefully almost
    // always avoid internal realloc() in string buffer
    size_t cap = strlen(input) * 3 / 2;

    if (!sc_strbuf_init(&buf, cap)) {
        return false;
    }

#define APPEND(S,N) if (!sc_strbuf_append(&buf, S, N)) goto error
#define APPEND_CHAR(C) if (!sc_strbuf_append_char(&buf, C)) goto error
#define APPEND_N(C,N) if (!sc_strbuf_append_n(&buf, C, N)) goto error
#define APPEND_INDENT() if (indent) APPEND_N(' ', indent)

    APPEND_INDENT();

    // The last separator encountered, it must be inserted only conditionally,
    // depending on the next token
    char pending = 0;

    // col tracks the current column in the current line
    size_t col = indent;
    while (*input) {
        size_t sep_idx = strcspn(input, "\n ");
        size_t new_col = col + sep_idx;
        if (pending == ' ') {
            // The pending space counts
            ++new_col;
        }
        bool wrap = new_col > columns;

        char sep = input[sep_idx];
        if (sep == ' ')
            sep = ' ';

        if (wrap) {
            APPEND_CHAR('\n');
            APPEND_INDENT();
            col = indent;
        } else if (pending) {
            APPEND_CHAR(pending);
            ++col;
            if (pending == '\n')
            {
                APPEND_INDENT();
                col = indent;
            }
        }

        if (sep_idx) {
            APPEND(input, sep_idx);
            col += sep_idx;
        }

        pending = sep;

        input += sep_idx;
        if (*input != '\0') {
            // Skip the separator
            ++input;
        }
    }

    if (pending)
        APPEND_CHAR(pending);

    return buf.s;

error:
    free(buf.s);
    return NULL;
}

ssize_t
sc_str_index_of_column(const char *s, unsigned col, const char *seps) {
    size_t colidx = 0;

    size_t idx = 0;
    while (s[idx] != '\0' && colidx != col) {
        size_t r = strcspn(&s[idx], seps);
        idx += r;

        if (s[idx] == '\0') {
            // Not found
            return -1;
        }

        size_t consecutive_seps = strspn(&s[idx], seps);
        assert(consecutive_seps); // At least one
        idx += consecutive_seps;

        if (s[idx] != '\0') {
            ++colidx;
        }
    }

    return col == colidx ? (ssize_t) idx : -1;
}

size_t
sc_str_remove_trailing_cr(char *s, size_t len) {
    while (len) {
        if (s[len - 1] != '\r') {
            break;
        }
        s[--len] = '\0';
    }
    return len;
}

char *
sc_str_to_hex_string(const uint8_t *data, size_t size) {
    size_t buffer_size = size * 3 + 1;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        LOG_OOM();
        return NULL;
    }

    for (size_t i = 0; i < size; ++i) {
        snprintf(buffer + i * 3, 4, "%02X ", data[i]);
    }

    // Remove the final space
    buffer[size * 3] = '\0';

    return buffer;
}
