#ifndef SC_DAEMON_PROTOCOL_H
#define SC_DAEMON_PROTOCOL_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/net.h"
#include "util/strbuf.h"

// Protocol version, bumped only on incompatible changes (see doc/daemon.md §8)
// v2: hello advertises add-on argument schemas ("plugins"); plugin op carries
//     named args ("arg_names"/"arg_values") beyond the primary value.
// v3: hello plugin schema is keyed ("name|key=value|..."); plugin op response
//     carries a "result" object; new "upload" op for remote byte transfer.
#define SC_DAEMON_PROTOCOL_VERSION 3

// Sanity cap for a frame (JSON document or binary payload)
#define SC_DAEMON_MAX_FRAME_SIZE (64 * 1024 * 1024) // 64 MiB

#define SC_DAEMON_MAX_JSON_TOKENS 128

// Opaque token type mirroring jsmntok_t (defined in protocol.c only, to keep
// jsmn out of other translation units)
struct sc_json_tok {
    int type;
    int start;
    int end;
    int size;
};

struct sc_json {
    const char *buf; // not owned
    struct sc_json_tok toks[SC_DAEMON_MAX_JSON_TOKENS];
    int ntok;
};

/**
 * Read one protocol frame: a 4-byte big-endian length followed by a JSON
 * document of that size.
 *
 * On success, `*out_json` receives a malloc'd NUL-terminated copy of the
 * document (owned by the caller).
 *
 * Returns false on EOF, I/O error or invalid/oversized length.
 */
bool
sc_daemon_read_json(sc_socket socket, char **out_json, size_t *out_len);

/**
 * Write one protocol frame: 4-byte big-endian length + JSON document,
 * followed by `payload_len` raw payload bytes (if any).
 */
bool
sc_daemon_write_frame(sc_socket socket, const char *json, size_t json_len,
                      const uint8_t *payload, size_t payload_len);

/**
 * Read exactly `len` payload bytes into a malloc'd buffer.
 */
bool
sc_daemon_read_payload(sc_socket socket, size_t len, uint8_t **out_data);

/**
 * Parse a JSON document (single object at top level).
 *
 * `buf` must remain valid while `json` is used.
 */
bool
sc_json_parse(struct sc_json *json, const char *buf, size_t len);

/**
 * Find the value token of a top-level key, or NULL.
 */
const struct sc_json_tok *
sc_json_get(const struct sc_json *json, const char *key);

/**
 * Get the raw JSON text of a top-level key's value (verbatim document
 * substring, including nested braces for an object/array value). Returns a
 * malloc'd NUL-terminated copy (owned by the caller), or NULL if absent.
 */
char *
sc_json_get_raw(const struct sc_json *json, const char *key);

/**
 * Get a top-level string value as a malloc'd unescaped copy (owned by the
 * caller). Returns false if absent or not a string.
 */
bool
sc_json_get_string(const struct sc_json *json, const char *key, char **out);

/**
 * Get a top-level integer value. Returns false if absent or not a number.
 */
bool
sc_json_get_int64(const struct sc_json *json, const char *key, int64_t *out);

/**
 * Get a top-level boolean value. Returns false if absent or not a boolean.
 */
bool
sc_json_get_bool(const struct sc_json *json, const char *key, bool *out);

/**
 * For a top-level key holding an array of strings, unescape each item into
 * `items` (each malloc'd, owned by the caller) up to `max`.
 * Returns false if absent, not an array, an item is not a string, or there
 * are more than `max` items.
 */
bool
sc_json_get_string_array(const struct sc_json *json, const char *key,
                         char **items, unsigned max, unsigned *out_count);

/**
 * Append a JSON-escaped string (including surrounding quotes) to `buf`.
 */
bool
sc_json_append_escaped(struct sc_strbuf *buf, const char *s);

#endif
