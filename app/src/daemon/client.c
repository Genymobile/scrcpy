#include "client.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daemon/protocol.h"
#include "util/log.h"
#include "util/net.h"
#include "util/strbuf.h"

// Read and parse one response/event frame; on success, `*out_json` (malloc'd
// document backing `json`) must be freed by the caller
static bool
client_read(sc_socket socket, struct sc_json *json, char **out_json) {
    char *doc;
    size_t len;
    if (!sc_daemon_read_json(socket, &doc, &len)) {
        LOGE("Client: connection to daemon lost");
        return false;
    }
    if (!sc_json_parse(json, doc, len)) {
        LOGE("Client: invalid response from daemon");
        free(doc);
        return false;
    }
    *out_json = doc;
    return true;
}

// Return true if the response reports ok:true. On failure, print the raw
// response (it contains the error object).
static bool
response_ok(const struct sc_json *json, const char *doc) {
    bool ok;
    if (sc_json_get_bool(json, "ok", &ok) && ok) {
        return true;
    }
    LOGE("Client: request failed: %s", doc);
    return false;
}

static bool
send_request(sc_socket socket, const char *json) {
    return sc_daemon_write_frame(socket, json, strlen(json), NULL, 0);
}

// Append the optional ,"action":"..." field (for --auto-test-report)
static bool
append_action(struct sc_strbuf *buf, const struct scrcpy_options *opts) {
    if (!opts->action) {
        return true;
    }
    return sc_strbuf_append_staticstr(buf, ",\"action\":")
        && sc_json_append_escaped(buf, opts->action);
}

static bool
do_control(sc_socket socket, const struct scrcpy_options *opts, int64_t id) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        return false;
    }

    char head[64];
    snprintf(head, sizeof(head), "{\"id\":%" PRId64 ",\"op\":\"control\","
                                 "\"cmds\":[", id);
    bool w = sc_strbuf_append_str(&buf, head);
    for (unsigned i = 0; w && i < opts->control_cmd_count; ++i) {
        if (i) {
            w = sc_strbuf_append_char(&buf, ',');
        }
        w = w && sc_json_append_escaped(&buf, opts->control_cmds[i]);
    }
    w = w && sc_strbuf_append_char(&buf, ']');
    w = w && append_action(&buf, opts);
    w = w && sc_strbuf_append_char(&buf, '}');
    if (!w) {
        free(buf.s);
        return false;
    }

    bool sent = sc_daemon_write_frame(socket, buf.s, buf.len, NULL, 0);
    free(buf.s);
    if (!sent) {
        LOGE("Client: could not send control request");
        return false;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(socket, &json, &doc)) {
        return false;
    }
    bool ok = response_ok(&json, doc);
    free(doc);
    return ok;
}

static bool
do_screencap(sc_socket socket, const struct scrcpy_options *opts,
             int64_t id) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 128)) {
        return false;
    }
    char head[64];
    snprintf(head, sizeof(head),
             "{\"id\":%" PRId64 ",\"op\":\"screencap\",\"format\":\"png\"",
             id);
    bool w = sc_strbuf_append_str(&buf, head)
          && append_action(&buf, opts)
          && sc_strbuf_append_char(&buf, '}');
    if (!w) {
        free(buf.s);
        return false;
    }
    bool sent = sc_daemon_write_frame(socket, buf.s, buf.len, NULL, 0);
    free(buf.s);
    if (!sent) {
        LOGE("Client: could not send screencap request");
        return false;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(socket, &json, &doc)) {
        return false;
    }

    bool ok = response_ok(&json, doc);
    int64_t payload_len = 0;
    if (ok && (!sc_json_get_int64(&json, "payload_len", &payload_len)
                   || payload_len <= 0)) {
        LOGE("Client: missing screencap payload");
        ok = false;
    }
    free(doc);
    if (!ok) {
        return false;
    }

    uint8_t *png;
    if (!sc_daemon_read_payload(socket, payload_len, &png)) {
        LOGE("Client: could not read screencap payload");
        return false;
    }

    ok = false;
    FILE *fp = fopen(opts->screencap_filename, "wb");
    if (!fp) {
        LOGE("Client: could not open output file: %s",
             opts->screencap_filename);
        goto end;
    }
    size_t written = fwrite(png, 1, payload_len, fp);
    fclose(fp);
    if (written != (size_t) payload_len) {
        LOGE("Client: failed to write %s", opts->screencap_filename);
        goto end;
    }

    LOGI("Screenshot saved to %s", opts->screencap_filename);
    ok = true;

end:
    free(png);
    return ok;
}

static bool
do_note(sc_socket socket, const struct scrcpy_options *opts, int64_t id) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 128)) {
        return false;
    }
    char head[48];
    snprintf(head, sizeof(head), "{\"id\":%" PRId64 ",\"op\":\"note\",\"note\":",
             id);
    bool w = sc_strbuf_append_str(&buf, head)
          && sc_json_append_escaped(&buf, opts->note)
          && sc_strbuf_append_char(&buf, '}');
    if (!w) {
        free(buf.s);
        return false;
    }
    bool sent = sc_daemon_write_frame(socket, buf.s, buf.len, NULL, 0);
    free(buf.s);
    if (!sent) {
        LOGE("Client: could not send note");
        return false;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(socket, &json, &doc)) {
        return false;
    }
    bool ok = response_ok(&json, doc);
    free(doc);
    if (ok) {
        LOGI("Note recorded");
    }
    return ok;
}

// Invoke a plugin command (doc/addons.md): the daemon runs the registered
// add-on for `name` with `args`. The add-on's own screenshot/note/click steps
// come back as their own requests, so this blocks until the add-on finishes.
static bool
do_plugin(sc_socket socket, const char *name, const char *args, int64_t id) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 128)) {
        return false;
    }
    char head[48];
    snprintf(head, sizeof(head), "{\"id\":%" PRId64 ",\"op\":\"plugin\","
                                 "\"name\":", id);
    bool w = sc_strbuf_append_str(&buf, head)
          && sc_json_append_escaped(&buf, name)
          && sc_strbuf_append_staticstr(&buf, ",\"args\":")
          && sc_json_append_escaped(&buf, args ? args : "")
          && sc_strbuf_append_char(&buf, '}');
    if (!w) {
        free(buf.s);
        return false;
    }
    bool sent = sc_daemon_write_frame(socket, buf.s, buf.len, NULL, 0);
    free(buf.s);
    if (!sent) {
        LOGE("Client: could not send plugin request");
        return false;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(socket, &json, &doc)) {
        return false;
    }
    bool ok = response_ok(&json, doc);
    free(doc);
    return ok;
}

static bool
do_status(sc_socket socket, int64_t id) {
    char req[64];
    snprintf(req, sizeof(req), "{\"id\":%" PRId64 ",\"op\":\"status\"}", id);
    if (!send_request(socket, req)) {
        return false;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(socket, &json, &doc)) {
        return false;
    }

    bool ok = response_ok(&json, doc);
    if (ok) {
        // Raw JSON on stdout, for both humans and scripts
        printf("%s\n", doc);
    }
    free(doc);
    return ok;
}

static bool
do_shutdown(sc_socket socket, int64_t id) {
    char req[64];
    snprintf(req, sizeof(req), "{\"id\":%" PRId64 ",\"op\":\"shutdown\"}", id);
    if (!send_request(socket, req)) {
        return false;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(socket, &json, &doc)) {
        return false;
    }
    bool ok = response_ok(&json, doc);
    free(doc);
    if (ok) {
        LOGI("Daemon shutdown requested");
    }
    return ok;
}

enum scrcpy_exit_code
sc_client_run(const struct scrcpy_options *opts) {
    uint16_t port = opts->client_port;

    sc_socket socket = net_socket();
    if (socket == SC_SOCKET_NONE) {
        return SCRCPY_EXIT_FAILURE;
    }

    if (!net_connect(socket, IPV4_LOCALHOST, port)) {
        LOGE("Client: could not connect to daemon on 127.0.0.1:%" PRIu16
             " (is it running?)", port);
        net_close(socket);
        return SCRCPY_EXIT_DISCONNECTED;
    }

    net_set_tcp_nodelay(socket, true);

    enum scrcpy_exit_code ret = SCRCPY_EXIT_DISCONNECTED;

    // The daemon speaks first (doc/daemon.md §8.2)
    struct sc_json hello;
    char *hello_doc;
    if (!client_read(socket, &hello, &hello_doc)) {
        goto end;
    }

    int64_t protocol = 0;
    bool proto_ok = sc_json_get_int64(&hello, "protocol", &protocol)
                 && protocol == SC_DAEMON_PROTOCOL_VERSION;
    if (!proto_ok) {
        LOGE("Client: daemon protocol mismatch (daemon: %" PRId64
             ", client: %d); update scrcpy-auto on both sides",
             protocol, SC_DAEMON_PROTOCOL_VERSION);
        free(hello_doc);
        goto end;
    }

    char *state = NULL;
    char *serial = NULL;
    sc_json_get_string(&hello, "state", &state);
    sc_json_get_string(&hello, "serial", &serial);
    LOGI("Connected to daemon on port %" PRIu16 " (device: %s, state: %s)",
         port, serial && *serial ? serial : "?", state ? state : "?");
    free(state);
    free(serial);
    free(hello_doc);

    // Execute requested operations in order: note, control, screencap,
    // status, shutdown (doc/daemon.md §8.6)
    int64_t id = 1;
    ret = SCRCPY_EXIT_SUCCESS;

    if (opts->note) {
        if (!do_note(socket, opts, id++)) {
            ret = SCRCPY_EXIT_FAILURE;
            goto end;
        }
    }

    for (unsigned i = 0; i < opts->plugin_count; ++i) {
        if (!do_plugin(socket, opts->plugin_names[i], opts->plugin_args[i],
                       id++)) {
            ret = SCRCPY_EXIT_FAILURE;
            goto end;
        }
    }

    if (opts->control_cmd_count) {
        if (!do_control(socket, opts, id++)) {
            ret = SCRCPY_EXIT_FAILURE;
            goto end;
        }
    }

    if (opts->screencap_filename) {
        if (!do_screencap(socket, opts, id++)) {
            ret = SCRCPY_EXIT_FAILURE;
            goto end;
        }
    }

    if (opts->daemon_status) {
        if (!do_status(socket, id++)) {
            ret = SCRCPY_EXIT_FAILURE;
            goto end;
        }
    }

    if (opts->daemon_stop) {
        if (!do_shutdown(socket, id++)) {
            ret = SCRCPY_EXIT_FAILURE;
            goto end;
        }
    }

end:
    net_close(socket);
    return ret;
}
