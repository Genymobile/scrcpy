#include "protocol.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STATIC
#include "third_party/jsmn.h"

#include "util/binary.h"
#include "util/log.h"

static bool
recv_all(sc_socket socket, void *buf, size_t len) {
    char *p = buf;
    while (len > 0) {
        ssize_t r = net_recv_all(socket, p, len);
        if (r <= 0) {
            return false;
        }
        assert((size_t) r <= len);
        p += r;
        len -= r;
    }
    return true;
}

bool
sc_daemon_read_json(sc_socket socket, char **out_json, size_t *out_len) {
    uint8_t header[4];
    if (!recv_all(socket, header, sizeof(header))) {
        return false;
    }

    uint32_t len = sc_read32be(header);
    if (!len || len > SC_DAEMON_MAX_FRAME_SIZE) {
        LOGW("Daemon protocol: invalid frame length: %" PRIu32, len);
        return false;
    }

    char *json = malloc(len + 1);
    if (!json) {
        LOG_OOM();
        return false;
    }

    if (!recv_all(socket, json, len)) {
        free(json);
        return false;
    }
    json[len] = '\0';

    *out_json = json;
    *out_len = len;
    return true;
}

bool
sc_daemon_write_frame(sc_socket socket, const char *json, size_t json_len,
                      const uint8_t *payload, size_t payload_len) {
    assert(json_len && json_len <= SC_DAEMON_MAX_FRAME_SIZE);
    assert(payload_len <= SC_DAEMON_MAX_FRAME_SIZE);

    uint8_t header[4];
    sc_write32be(header, (uint32_t) json_len);

    if (net_send_all(socket, header, sizeof(header)) != sizeof(header)) {
        return false;
    }
    if (net_send_all(socket, json, json_len) != (ssize_t) json_len) {
        return false;
    }
    if (payload_len
            && net_send_all(socket, payload, payload_len)
                    != (ssize_t) payload_len) {
        return false;
    }
    return true;
}

bool
sc_daemon_read_payload(sc_socket socket, size_t len, uint8_t **out_data) {
    if (!len || len > SC_DAEMON_MAX_FRAME_SIZE) {
        return false;
    }

    uint8_t *data = malloc(len);
    if (!data) {
        LOG_OOM();
        return false;
    }

    if (!recv_all(socket, data, len)) {
        free(data);
        return false;
    }

    *out_data = data;
    return true;
}

bool
sc_json_parse(struct sc_json *json, const char *buf, size_t len) {
    jsmn_parser parser;
    jsmn_init(&parser);

    jsmntok_t toks[SC_DAEMON_MAX_JSON_TOKENS];
    int n = jsmn_parse(&parser, buf, len, toks, SC_DAEMON_MAX_JSON_TOKENS);
    if (n < 1 || toks[0].type != JSMN_OBJECT) {
        return false;
    }

    json->buf = buf;
    json->ntok = n;
    for (int i = 0; i < n; ++i) {
        json->toks[i].type = toks[i].type;
        json->toks[i].start = toks[i].start;
        json->toks[i].end = toks[i].end;
        json->toks[i].size = toks[i].size;
    }
    return true;
}

// Number of tokens spanned by the value at index i (including itself)
static int
tok_span(const struct sc_json *json, int i) {
    int count = 1;
    int children = json->toks[i].size;
    for (int c = 0; c < children; ++c) {
        count += tok_span(json, i + count);
    }
    return count;
}

const struct sc_json_tok *
sc_json_get(const struct sc_json *json, const char *key) {
    size_t key_len = strlen(key);
    // Token 0 is the top-level object; iterate over its key/value pairs
    int i = 1;
    int pairs = json->toks[0].size;
    for (int p = 0; p < pairs && i + 1 < json->ntok; ++p) {
        const struct sc_json_tok *k = &json->toks[i];
        int vlen = tok_span(json, i + 1);
        if (k->type == JSMN_STRING
                && (size_t) (k->end - k->start) == key_len
                && !memcmp(json->buf + k->start, key, key_len)) {
            return &json->toks[i + 1];
        }
        i += 1 + vlen;
    }
    return NULL;
}

// Unescape a JSON string token into a malloc'd NUL-terminated buffer
static char *
unescape(const char *s, size_t len) {
    // Unescaping never grows the string
    char *out = malloc(len + 1);
    if (!out) {
        LOG_OOM();
        return NULL;
    }

    size_t o = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        if (c != '\\') {
            out[o++] = c;
            continue;
        }
        if (++i == len) {
            break; // trailing backslash: ignore
        }
        switch (s[i]) {
            case '"': out[o++] = '"'; break;
            case '\\': out[o++] = '\\'; break;
            case '/': out[o++] = '/'; break;
            case 'b': out[o++] = '\b'; break;
            case 'f': out[o++] = '\f'; break;
            case 'n': out[o++] = '\n'; break;
            case 'r': out[o++] = '\r'; break;
            case 't': out[o++] = '\t'; break;
            case 'u': {
                if (i + 4 >= len) {
                    i = len; // malformed: stop
                    break;
                }
                char hex[5] = {s[i+1], s[i+2], s[i+3], s[i+4], '\0'};
                i += 4;
                unsigned long cp = strtoul(hex, NULL, 16);
                // Combine surrogate pairs
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < len
                        && s[i+1] == '\\' && s[i+2] == 'u') {
                    char hex2[5] = {s[i+3], s[i+4], s[i+5], s[i+6], '\0'};
                    unsigned long lo = strtoul(hex2, NULL, 16);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        i += 6;
                    }
                }
                // Encode as UTF-8
                if (cp < 0x80) {
                    out[o++] = (char) cp;
                } else if (cp < 0x800) {
                    out[o++] = (char) (0xC0 | (cp >> 6));
                    out[o++] = (char) (0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    out[o++] = (char) (0xE0 | (cp >> 12));
                    out[o++] = (char) (0x80 | ((cp >> 6) & 0x3F));
                    out[o++] = (char) (0x80 | (cp & 0x3F));
                } else {
                    out[o++] = (char) (0xF0 | (cp >> 18));
                    out[o++] = (char) (0x80 | ((cp >> 12) & 0x3F));
                    out[o++] = (char) (0x80 | ((cp >> 6) & 0x3F));
                    out[o++] = (char) (0x80 | (cp & 0x3F));
                }
                break;
            }
            default:
                out[o++] = s[i]; // unknown escape: keep as-is
                break;
        }
    }
    out[o] = '\0';
    return out;
}

bool
sc_json_get_string(const struct sc_json *json, const char *key, char **out) {
    const struct sc_json_tok *tok = sc_json_get(json, key);
    if (!tok || tok->type != JSMN_STRING) {
        return false;
    }

    char *s = unescape(json->buf + tok->start, tok->end - tok->start);
    if (!s) {
        return false;
    }
    *out = s;
    return true;
}

bool
sc_json_get_int64(const struct sc_json *json, const char *key, int64_t *out) {
    const struct sc_json_tok *tok = sc_json_get(json, key);
    if (!tok || tok->type != JSMN_PRIMITIVE) {
        return false;
    }

    char c = json->buf[tok->start];
    if (c != '-' && (c < '0' || c > '9')) {
        return false; // true/false/null
    }

    char tmp[24];
    size_t len = tok->end - tok->start;
    if (len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, json->buf + tok->start, len);
    tmp[len] = '\0';

    char *end;
    long long value = strtoll(tmp, &end, 10);
    if (end == tmp) {
        return false;
    }
    *out = value;
    return true;
}

bool
sc_json_get_bool(const struct sc_json *json, const char *key, bool *out) {
    const struct sc_json_tok *tok = sc_json_get(json, key);
    if (!tok || tok->type != JSMN_PRIMITIVE) {
        return false;
    }

    size_t len = tok->end - tok->start;
    if (len == 4 && !memcmp(json->buf + tok->start, "true", 4)) {
        *out = true;
        return true;
    }
    if (len == 5 && !memcmp(json->buf + tok->start, "false", 5)) {
        *out = false;
        return true;
    }
    return false;
}

bool
sc_json_get_string_array(const struct sc_json *json, const char *key,
                         char **items, unsigned max, unsigned *out_count) {
    const struct sc_json_tok *tok = sc_json_get(json, key);
    if (!tok || tok->type != JSMN_ARRAY) {
        return false;
    }

    unsigned count = tok->size;
    if (count > max) {
        return false;
    }

    // Array items follow the array token; strings have no children so they
    // are consecutive tokens
    int idx = (int) (tok - json->toks) + 1;
    for (unsigned i = 0; i < count; ++i) {
        const struct sc_json_tok *item = &json->toks[idx + (int) i];
        if (item->type != JSMN_STRING) {
            goto error;
        }
        items[i] = unescape(json->buf + item->start, item->end - item->start);
        if (!items[i]) {
            goto error;
        }
        continue;
error:
        for (unsigned j = 0; j < i; ++j) {
            free(items[j]);
        }
        return false;
    }

    *out_count = count;
    return true;
}

bool
sc_json_append_escaped(struct sc_strbuf *buf, const char *s) {
    if (!sc_strbuf_append_char(buf, '"')) {
        return false;
    }
    for (; *s; ++s) {
        unsigned char c = (unsigned char) *s;
        bool ok;
        switch (c) {
            case '"': ok = sc_strbuf_append_staticstr(buf, "\\\""); break;
            case '\\': ok = sc_strbuf_append_staticstr(buf, "\\\\"); break;
            case '\b': ok = sc_strbuf_append_staticstr(buf, "\\b"); break;
            case '\f': ok = sc_strbuf_append_staticstr(buf, "\\f"); break;
            case '\n': ok = sc_strbuf_append_staticstr(buf, "\\n"); break;
            case '\r': ok = sc_strbuf_append_staticstr(buf, "\\r"); break;
            case '\t': ok = sc_strbuf_append_staticstr(buf, "\\t"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    ok = sc_strbuf_append_str(buf, esc);
                } else {
                    ok = sc_strbuf_append_char(buf, (char) c);
                }
                break;
        }
        if (!ok) {
            return false;
        }
    }
    return sc_strbuf_append_char(buf, '"');
}
