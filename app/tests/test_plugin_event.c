#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <process.h>
# define getpid _getpid
#else
# include <unistd.h>
#endif

#include "daemon/plugin_event.h"

static void
test_serialize_escape_and_active_duration(void) {
    const struct sc_plugin_named_input named[] = {
        {"ref\"name", "line 1\nline 2"},
        {"mode", "a\\b"},
    };
    const char *assets[] = {
        "assets/0-\"reference\".png",
        "assets/1-back\\slash.png",
    };
    struct sc_plugin_completion_event event = {
        .name = "vision\"plugin",
        .args = "primary\nvalue",
        .named = named,
        .named_count = ARRAY_LEN(named),
        .assets = assets,
        .asset_count = ARRAY_LEN(assets),
        .start_ms = 123,
        .end_ms = 2234,
        .duration_ms = 2111,
        .result_json =
            "{\"ok\":true,\"active_duration_ms\":345,\"nested\":{\"x\":1}}",
        .status = "ready",
        .has_exit_code = true,
        .exit_code = 0,
        .service = true,
        .adopted = true,
    };

    char *json;
    assert(sc_plugin_event_serialize_extra(&event, &json));
    assert(!strcmp(json,
        "\"name\":\"vision\\\"plugin\","
        "\"inputs\":{\"args\":\"primary\\nvalue\","
        "\"named\":{\"ref\\\"name\":\"line 1\\nline 2\","
        "\"mode\":\"a\\\\b\"}},"
        "\"assets\":[\"assets/0-\\\"reference\\\".png\","
        "\"assets/1-back\\\\slash.png\"],"
        "\"start_ms\":123,\"duration_ms\":2111,"
        "\"result\":{\"ok\":true,\"active_duration_ms\":345,"
        "\"nested\":{\"x\":1}},"
        "\"status\":\"ready\",\"exit_code\":0,"
        "\"service\":true,\"adopted\":true,"
        "\"active_duration_ms\":345"));
    assert(!strstr(json, "\"op\""));
    assert(!strstr(json, "\"t_ms\""));
    assert(!strstr(json, "\"wall\""));
    assert(!strstr(json, "\"end_ms\""));
    free(json);
}

static void
test_serialize_nulls(void) {
    struct sc_plugin_completion_event event = {
        .name = "empty",
        .start_ms = 9,
        .end_ms = 9,
        .duration_ms = 0,
        .status = "start_failed",
    };

    char *json;
    assert(sc_plugin_event_serialize_extra(&event, &json));
    assert(!strcmp(json,
        "\"name\":\"empty\","
        "\"inputs\":{\"args\":\"\",\"named\":{}},"
        "\"assets\":[],\"start_ms\":9,\"duration_ms\":0,"
        "\"result\":null,\"status\":\"start_failed\",\"exit_code\":null,"
        "\"service\":false,\"adopted\":false"));
    assert(!strstr(json, "active_duration_ms"));
    free(json);
}

static void
test_timing_is_exact_and_threshold_free(void) {
    struct sc_plugin_completion_event event = {
        .name = "timing",
        .start_ms = 1001,
        .end_ms = 3000,
        .duration_ms = 1999,
        .status = "ok",
    };

    char *json;
    assert(sc_plugin_event_serialize_extra(&event, &json));
    assert(strstr(json, "\"start_ms\":1001"));
    assert(strstr(json, "\"duration_ms\":1999"));
    free(json);

    event.end_ms = 3001;
    event.duration_ms = 2000;
    assert(sc_plugin_event_serialize_extra(&event, &json));
    assert(strstr(json, "\"start_ms\":1001"));
    assert(strstr(json, "\"duration_ms\":2000"));
    free(json);

    // Serialization must reject, not repair, an inconsistent endpoint.
    event.duration_ms = 1999;
    json = (char *) 0x1;
    assert(!sc_plugin_event_serialize_extra(&event, &json));
    assert(!json);
}

static void
make_test_path(char *path, size_t size) {
    snprintf(path, size, "test-plugin-result-%ld.json", (long) getpid());
}

static void
write_file(const char *path, const char *data, size_t len) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(data, 1, len, file) == len);
    assert(fclose(file) == 0);
}

static void
test_result_file_states(void) {
    char path[128];
    make_test_path(path, sizeof(path));
    remove(path);

    struct sc_plugin_result result;
    enum sc_plugin_result_status status =
        sc_plugin_result_read(path, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_MISSING);
    sc_plugin_result_destroy(&result);

    write_file(path, "", 0);
    status = sc_plugin_result_read(path, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_EMPTY);
    sc_plugin_result_destroy(&result);

    // A service may expose its result path before its write is complete.
    static const char partial[] =
        "{\"ready\":true,\"active_duration_ms\":";
    write_file(path, partial, sizeof(partial) - 1);
    status = sc_plugin_result_read(path, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_INVALID);
    sc_plugin_result_destroy(&result);

    static const char valid[] =
        "{\"ready\":true,\"active_duration_ms\":17,\"items\":[1,2]}";
    write_file(path, valid, sizeof(valid) - 1);
    status = sc_plugin_result_read(path, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_VALID);
    assert(result.json);
    assert(!strcmp(result.json, valid));
    assert(result.has_active_duration_ms);
    assert(result.active_duration_ms == 17);
    sc_plugin_result_destroy(&result);

    // The cap is exact: exactly 32 bytes is permitted if valid.
    static const char exactly_capped[] =
        "{\"padding\":\"123456789012345678\"}";
    assert(sizeof(exactly_capped) - 1 == 32);
    write_file(path, exactly_capped, sizeof(exactly_capped) - 1);
    status = sc_plugin_result_read(path, 32, &result);
    assert(status == SC_PLUGIN_RESULT_VALID);
    assert(!strcmp(result.json, exactly_capped));
    sc_plugin_result_destroy(&result);

    // Byte 33 is rejected before JSON parsing.
    static const char oversized[] = "123456789012345678901234567890123";
    assert(sizeof(oversized) - 1 == 33);
    write_file(path, oversized, sizeof(oversized) - 1);
    status = sc_plugin_result_read(path, 32, &result);
    assert(status == SC_PLUGIN_RESULT_TOO_LARGE);
    sc_plugin_result_destroy(&result);

    assert(remove(path) == 0);

    status = sc_plugin_result_read(NULL, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_IO_ERROR);
    sc_plugin_result_destroy(&result);
}

static void
test_result_validation_and_active_duration_rules(void) {
    char path[128];
    make_test_path(path, sizeof(path));

    static const char invalid_trailing_comma[] = "{\"ok\":true,}";
    write_file(path, invalid_trailing_comma,
               sizeof(invalid_trailing_comma) - 1);
    struct sc_plugin_result result;
    enum sc_plugin_result_status status =
        sc_plugin_result_read(path, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_INVALID);
    sc_plugin_result_destroy(&result);

    static const char negative[] =
        "{\"active_duration_ms\":-1,\"ok\":true}";
    write_file(path, negative, sizeof(negative) - 1);
    status = sc_plugin_result_read(path, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_VALID);
    assert(!result.has_active_duration_ms);
    sc_plugin_result_destroy(&result);

    static const char fractional[] =
        "{\"active_duration_ms\":1.5,\"ok\":true}";
    write_file(path, fractional, sizeof(fractional) - 1);
    status = sc_plugin_result_read(path, SC_PLUGIN_RESULT_MAX_SIZE, &result);
    assert(status == SC_PLUGIN_RESULT_VALID);
    assert(!result.has_active_duration_ms);
    sc_plugin_result_destroy(&result);

    assert(remove(path) == 0);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_serialize_escape_and_active_duration();
    test_serialize_nulls();
    test_timing_is_exact_and_threshold_free();
    test_result_file_states();
    test_result_validation_and_active_duration_rules();
    return 0;
}
