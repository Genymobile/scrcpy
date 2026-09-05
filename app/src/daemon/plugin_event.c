#include "plugin_event.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STATIC
#define JSMN_STRICT
#include "third_party/jsmn.h"

#include "util/strbuf.h"

#define SC_PLUGIN_JSON_MAX_DEPTH 512

struct sc_plugin_json_cursor {
    const char *s;
    size_t len;
    size_t pos;
};

static void
skip_whitespace(struct sc_plugin_json_cursor *cursor) {
    while (cursor->pos < cursor->len) {
        char c = cursor->s[cursor->pos];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        ++cursor->pos;
    }
}

static bool
is_hex_digit(char c) {
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

static bool
parse_json_string(struct sc_plugin_json_cursor *cursor) {
    if (cursor->pos == cursor->len || cursor->s[cursor->pos] != '"') {
        return false;
    }
    ++cursor->pos;
    while (cursor->pos < cursor->len) {
        unsigned char c = (unsigned char) cursor->s[cursor->pos++];
        if (c == '"') {
            return true;
        }
        if (c < 0x20) {
            return false;
        }
        if (c != '\\') {
            continue;
        }
        if (cursor->pos == cursor->len) {
            return false;
        }
        char escaped = cursor->s[cursor->pos++];
        if (escaped == '"' || escaped == '\\' || escaped == '/'
                || escaped == 'b' || escaped == 'f' || escaped == 'n'
                || escaped == 'r' || escaped == 't') {
            continue;
        }
        if (escaped != 'u' || cursor->len - cursor->pos < 4) {
            return false;
        }
        for (unsigned i = 0; i < 4; ++i) {
            if (!is_hex_digit(cursor->s[cursor->pos++])) {
                return false;
            }
        }
    }
    return false;
}

static bool
parse_json_number(struct sc_plugin_json_cursor *cursor) {
    size_t pos = cursor->pos;
    if (pos < cursor->len && cursor->s[pos] == '-') {
        ++pos;
    }
    if (pos == cursor->len) {
        return false;
    }

    if (cursor->s[pos] == '0') {
        ++pos;
        // Leading zeroes are not valid JSON numbers.
        if (pos < cursor->len
                && cursor->s[pos] >= '0' && cursor->s[pos] <= '9') {
            return false;
        }
    } else if (cursor->s[pos] >= '1' && cursor->s[pos] <= '9') {
        do {
            ++pos;
        } while (pos < cursor->len
                 && cursor->s[pos] >= '0' && cursor->s[pos] <= '9');
    } else {
        return false;
    }

    if (pos < cursor->len && cursor->s[pos] == '.') {
        ++pos;
        size_t fraction_start = pos;
        while (pos < cursor->len
                && cursor->s[pos] >= '0' && cursor->s[pos] <= '9') {
            ++pos;
        }
        if (pos == fraction_start) {
            return false;
        }
    }

    if (pos < cursor->len
            && (cursor->s[pos] == 'e' || cursor->s[pos] == 'E')) {
        ++pos;
        if (pos < cursor->len
                && (cursor->s[pos] == '+' || cursor->s[pos] == '-')) {
            ++pos;
        }
        size_t exponent_start = pos;
        while (pos < cursor->len
                && cursor->s[pos] >= '0' && cursor->s[pos] <= '9') {
            ++pos;
        }
        if (pos == exponent_start) {
            return false;
        }
    }

    cursor->pos = pos;
    return true;
}

static bool
consume_literal(struct sc_plugin_json_cursor *cursor, const char *literal) {
    size_t len = strlen(literal);
    if (cursor->len - cursor->pos < len
            || memcmp(&cursor->s[cursor->pos], literal, len)) {
        return false;
    }
    cursor->pos += len;
    return true;
}

static bool
parse_json_value(struct sc_plugin_json_cursor *cursor, unsigned depth);

static bool
parse_json_array(struct sc_plugin_json_cursor *cursor, unsigned depth) {
    ++cursor->pos; // '['
    skip_whitespace(cursor);
    if (cursor->pos < cursor->len && cursor->s[cursor->pos] == ']') {
        ++cursor->pos;
        return true;
    }

    for (;;) {
        if (!parse_json_value(cursor, depth + 1)) {
            return false;
        }
        skip_whitespace(cursor);
        if (cursor->pos == cursor->len) {
            return false;
        }
        char c = cursor->s[cursor->pos++];
        if (c == ']') {
            return true;
        }
        if (c != ',') {
            return false;
        }
        skip_whitespace(cursor);
        // Do not accept a trailing comma.
        if (cursor->pos == cursor->len || cursor->s[cursor->pos] == ']') {
            return false;
        }
    }
}

static bool
parse_json_object(struct sc_plugin_json_cursor *cursor, unsigned depth) {
    ++cursor->pos; // '{'
    skip_whitespace(cursor);
    if (cursor->pos < cursor->len && cursor->s[cursor->pos] == '}') {
        ++cursor->pos;
        return true;
    }

    for (;;) {
        if (!parse_json_string(cursor)) {
            return false;
        }
        skip_whitespace(cursor);
        if (cursor->pos == cursor->len || cursor->s[cursor->pos++] != ':') {
            return false;
        }
        if (!parse_json_value(cursor, depth + 1)) {
            return false;
        }
        skip_whitespace(cursor);
        if (cursor->pos == cursor->len) {
            return false;
        }
        char c = cursor->s[cursor->pos++];
        if (c == '}') {
            return true;
        }
        if (c != ',') {
            return false;
        }
        skip_whitespace(cursor);
        // Do not accept a trailing comma.
        if (cursor->pos == cursor->len || cursor->s[cursor->pos] == '}') {
            return false;
        }
    }
}

static bool
parse_json_value(struct sc_plugin_json_cursor *cursor, unsigned depth) {
    if (depth > SC_PLUGIN_JSON_MAX_DEPTH) {
        return false;
    }
    skip_whitespace(cursor);
    if (cursor->pos == cursor->len) {
        return false;
    }

    switch (cursor->s[cursor->pos]) {
        case '{':
            return parse_json_object(cursor, depth);
        case '[':
            return parse_json_array(cursor, depth);
        case '"':
            return parse_json_string(cursor);
        case 't':
            return consume_literal(cursor, "true");
        case 'f':
            return consume_literal(cursor, "false");
        case 'n':
            return consume_literal(cursor, "null");
        default:
            return parse_json_number(cursor);
    }
}

static bool
is_valid_json_object(const char *json, size_t len) {
    struct sc_plugin_json_cursor cursor = {
        .s = json,
        .len = len,
        .pos = 0,
    };
    skip_whitespace(&cursor);
    if (cursor.pos == cursor.len || cursor.s[cursor.pos] != '{'
            || !parse_json_object(&cursor, 0)) {
        return false;
    }
    skip_whitespace(&cursor);
    return cursor.pos == cursor.len;
}

static int
json_token_span(const jsmntok_t *tokens, int index) {
    int count = 1;
    for (int i = 0; i < tokens[index].size; ++i) {
        count += json_token_span(tokens, index + count);
    }
    return count;
}

static bool
parse_nonnegative_i64(const char *json, const jsmntok_t *token,
                      int64_t *out) {
    if (token->type != JSMN_PRIMITIVE) {
        return false;
    }
    size_t len = (size_t) (token->end - token->start);
    if (!len || len > 19 || json[token->start] < '0'
            || json[token->start] > '9') {
        return false;
    }
    char text[20];
    memcpy(text, &json[token->start], len);
    text[len] = '\0';

    errno = 0;
    char *end;
    long long value = strtoll(text, &end, 10);
    if (errno == ERANGE || *end || value < 0) {
        return false;
    }
    *out = (int64_t) value;
    return true;
}

static bool
extract_active_duration(const char *json, size_t len, bool *out_has_value,
                        int64_t *out_value) {
    *out_has_value = false;

    jsmn_parser parser;
    jsmn_init(&parser);
    int count = jsmn_parse(&parser, json, len, NULL, 0);
    if (count < 1 || (size_t) count > SIZE_MAX / sizeof(jsmntok_t)) {
        return false;
    }

    jsmntok_t *tokens = malloc((size_t) count * sizeof(*tokens));
    if (!tokens) {
        return false;
    }
    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, json, len, tokens, (unsigned) count);
    if (parsed != count || tokens[0].type != JSMN_OBJECT) {
        free(tokens);
        return false;
    }

    int index = 1;
    for (int i = 0; i < tokens[0].size; ++i) {
        const jsmntok_t *key = &tokens[index++];
        const jsmntok_t *value = &tokens[index];
        static const char field[] = "active_duration_ms";
        if (key->type == JSMN_STRING
                && key->end - key->start == (int) sizeof(field) - 1
                && !memcmp(&json[key->start], field, sizeof(field) - 1)
                && parse_nonnegative_i64(json, value, out_value)) {
            *out_has_value = true;
            break;
        }
        index += json_token_span(tokens, index);
    }
    free(tokens);
    return true;
}

static bool
append_json_escaped(struct sc_strbuf *buf, const char *s) {
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
                    char escaped[8];
                    snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                    ok = sc_strbuf_append_str(buf, escaped);
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

static bool
append_i64(struct sc_strbuf *buf, int64_t value) {
    char text[32];
    snprintf(text, sizeof(text), "%" PRId64, value);
    return sc_strbuf_append_str(buf, text);
}

bool
sc_plugin_event_serialize_extra(
        const struct sc_plugin_completion_event *event, char **out_json) {
    if (out_json) {
        *out_json = NULL;
    }
    if (!event || !out_json || !event->name || !event->status
            || event->start_ms < 0 || event->end_ms < event->start_ms
            || event->duration_ms < 0
            || event->duration_ms != event->end_ms - event->start_ms
            || (event->named_count && !event->named)
            || (event->asset_count && !event->assets)) {
        return false;
    }
    for (size_t i = 0; i < event->named_count; ++i) {
        if (!event->named[i].name || !event->named[i].value) {
            return false;
        }
    }
    for (size_t i = 0; i < event->asset_count; ++i) {
        if (!event->assets[i]) {
            return false;
        }
    }

    bool has_active_duration_ms = false;
    int64_t active_duration_ms = 0;
    if (event->result_json) {
        size_t len = strlen(event->result_json);
        if (!is_valid_json_object(event->result_json, len)) {
            return false;
        }
        if (!extract_active_duration(event->result_json, len,
                                     &has_active_duration_ms,
                                     &active_duration_ms)) {
            return false;
        }
    }

    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        return false;
    }

    bool ok = sc_strbuf_append_staticstr(&buf, "\"name\":")
           && append_json_escaped(&buf, event->name)
           && sc_strbuf_append_staticstr(&buf, ",\"inputs\":{\"args\":")
           && append_json_escaped(&buf, event->args ? event->args : "")
           && sc_strbuf_append_staticstr(&buf, ",\"named\":{");
    for (size_t i = 0; ok && i < event->named_count; ++i) {
        ok = (!i || sc_strbuf_append_char(&buf, ','))
          && append_json_escaped(&buf, event->named[i].name)
          && sc_strbuf_append_char(&buf, ':')
          && append_json_escaped(&buf, event->named[i].value);
    }
    ok = ok && sc_strbuf_append_staticstr(&buf, "}},\"assets\":[");
    for (size_t i = 0; ok && i < event->asset_count; ++i) {
        ok = (!i || sc_strbuf_append_char(&buf, ','))
          && append_json_escaped(&buf, event->assets[i]);
    }
    ok = ok && sc_strbuf_append_staticstr(&buf, "],\"start_ms\":")
       && append_i64(&buf, event->start_ms)
       && sc_strbuf_append_staticstr(&buf, ",\"duration_ms\":")
       && append_i64(&buf, event->duration_ms)
       && sc_strbuf_append_staticstr(&buf, ",\"result\":");
    if (ok) {
        ok = event->result_json
           ? sc_strbuf_append_str(&buf, event->result_json)
           : sc_strbuf_append_staticstr(&buf, "null");
    }
    ok = ok && sc_strbuf_append_staticstr(&buf, ",\"status\":")
       && append_json_escaped(&buf, event->status)
       && sc_strbuf_append_staticstr(&buf, ",\"exit_code\":");
    if (ok) {
        ok = event->has_exit_code
           ? append_i64(&buf, event->exit_code)
           : sc_strbuf_append_staticstr(&buf, "null");
    }
    ok = ok && sc_strbuf_append_staticstr(&buf, ",\"service\":")
       && sc_strbuf_append_str(&buf, event->service ? "true" : "false")
       && sc_strbuf_append_staticstr(&buf, ",\"adopted\":")
       && sc_strbuf_append_str(&buf, event->adopted ? "true" : "false");
    if (ok && has_active_duration_ms) {
        ok = sc_strbuf_append_staticstr(&buf, ",\"active_duration_ms\":")
          && append_i64(&buf, active_duration_ms);
    }

    if (!ok) {
        free(buf.s);
        return false;
    }
    *out_json = buf.s;
    return true;
}

enum sc_plugin_result_status
sc_plugin_result_read(const char *path, size_t max_size,
                      struct sc_plugin_result *out) {
    if (!out) {
        return SC_PLUGIN_RESULT_IO_ERROR;
    }
    out->json = NULL;
    out->has_active_duration_ms = false;
    out->active_duration_ms = 0;
    if (!path || max_size > SIZE_MAX - 2) {
        return SC_PLUGIN_RESULT_IO_ERROR;
    }

    errno = 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        return errno == ENOENT ? SC_PLUGIN_RESULT_MISSING
                              : SC_PLUGIN_RESULT_IO_ERROR;
    }

    // Read one byte past the cap so a file of max_size + 1 bytes cannot be
    // confused with an exactly capped file.
    size_t read_size = max_size + 1;
    char *json = malloc(read_size + 1);
    if (!json) {
        fclose(file);
        return SC_PLUGIN_RESULT_IO_ERROR;
    }
    size_t len = fread(json, 1, read_size, file);
    bool read_failed = ferror(file);
    bool close_failed = fclose(file) != 0;
    if (read_failed || close_failed) {
        free(json);
        return SC_PLUGIN_RESULT_IO_ERROR;
    }
    if (len > max_size) {
        free(json);
        return SC_PLUGIN_RESULT_TOO_LARGE;
    }
    if (!len) {
        free(json);
        return SC_PLUGIN_RESULT_EMPTY;
    }
    json[len] = '\0';
    if (!is_valid_json_object(json, len)) {
        free(json);
        return SC_PLUGIN_RESULT_INVALID;
    }

    if (!extract_active_duration(json, len, &out->has_active_duration_ms,
                                 &out->active_duration_ms)) {
        free(json);
        return SC_PLUGIN_RESULT_IO_ERROR;
    }
    out->json = json;
    return SC_PLUGIN_RESULT_VALID;
}

void
sc_plugin_result_destroy(struct sc_plugin_result *result) {
    if (!result) {
        return;
    }
    free(result->json);
    result->json = NULL;
    result->has_active_duration_ms = false;
    result->active_duration_ms = 0;
}
