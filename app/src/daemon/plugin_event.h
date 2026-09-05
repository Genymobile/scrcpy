#ifndef SC_DAEMON_PLUGIN_EVENT_H
#define SC_DAEMON_PLUGIN_EVENT_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Keep plugin result files bounded independently from daemon protocol frames.
#define SC_PLUGIN_RESULT_MAX_SIZE ((size_t) 256 * 1024)

struct sc_plugin_named_input {
    const char *name;
    const char *value;
};

/**
 * One completed plugin invocation.
 *
 * `end_ms` is intentionally not serialized by
 * sc_plugin_event_serialize_extra(): the report writer stores it as the
 * event's top-level `t_ms`. Likewise, the report writer owns `op` and `wall`.
 *
 * `duration_ms` must exactly equal `end_ms - start_ms`; the serializer rejects
 * inconsistent timing rather than silently moving a timeline endpoint.
 */
struct sc_plugin_completion_event {
    const char *name;
    const char *args; // optional primary input; NULL serializes as ""

    const struct sc_plugin_named_input *named;
    size_t named_count;
    const char *const *assets;
    size_t asset_count;

    int64_t start_ms;
    int64_t end_ms;
    int64_t duration_ms;

    const char *result_json; // optional valid top-level JSON object
    const char *status;
    bool has_exit_code;
    int64_t exit_code;
    bool service;
    bool adopted;
};

/**
 * Serialize the report "extra" fields for a completed plugin invocation.
 *
 * On success, `*out_json` receives a malloc'd NUL-terminated JSON object
 * interior (without surrounding braces), owned by the caller. It never
 * contains `op`, `t_ms` or `wall`.
 *
 * If `result_json` contains a non-negative top-level integer
 * `active_duration_ms`, that value is also copied to the top level of the
 * completion event for timeline renderers.
 */
bool
sc_plugin_event_serialize_extra(
    const struct sc_plugin_completion_event *event, char **out_json);

enum sc_plugin_result_status {
    SC_PLUGIN_RESULT_MISSING,
    SC_PLUGIN_RESULT_EMPTY,
    // Includes an incomplete write and any other malformed JSON object.
    SC_PLUGIN_RESULT_INVALID,
    SC_PLUGIN_RESULT_VALID,
    SC_PLUGIN_RESULT_TOO_LARGE,
    SC_PLUGIN_RESULT_IO_ERROR,
};

struct sc_plugin_result {
    // Set only for SC_PLUGIN_RESULT_VALID; owned by this struct.
    char *json;
    bool has_active_duration_ms;
    int64_t active_duration_ms;
};

/**
 * Read a plugin result file with a strict byte cap.
 *
 * Only a complete, valid top-level JSON object returns
 * SC_PLUGIN_RESULT_VALID, so service readiness polling may safely treat every
 * other status as "not ready". An incomplete file returns
 * SC_PLUGIN_RESULT_INVALID and may become valid on a later poll.
 *
 * `out` is always initialized and must eventually be passed to
 * sc_plugin_result_destroy() (it is also safe to destroy non-valid results).
 */
enum sc_plugin_result_status
sc_plugin_result_read(const char *path, size_t max_size,
                      struct sc_plugin_result *out);

void
sc_plugin_result_destroy(struct sc_plugin_result *result);

#endif
