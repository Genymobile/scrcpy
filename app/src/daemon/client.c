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

// Context threaded through the plugin invocation path.
struct plugin_ctx {
    sc_socket socket;
    const struct scrcpy_options *opts;
    bool local;          // daemon is loopback: pass paths; else upload bytes
    bool *json_printed;  // set once a JSON value has been emitted (--json)
    int64_t *id;         // shared request-id counter
};

// Invoke a plugin command (doc/addons.md): the daemon runs the registered
// add-on for `name` with `args`. The add-on's own screenshot/note/click steps
// come back as their own requests, so this blocks until the add-on finishes.
// Under --json, prints the plugin's result object (or a JSON error object).
static bool
do_plugin(struct plugin_ctx *ctx, const char *name, const char *args,
          const char *const *arg_names, const char *const *arg_values,
          unsigned arg_count, int64_t id) {
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
          && sc_json_append_escaped(&buf, args ? args : "");
    if (arg_count) {
        w = w && sc_strbuf_append_staticstr(&buf, ",\"arg_names\":[");
        for (unsigned i = 0; w && i < arg_count; ++i) {
            w = (i == 0 || sc_strbuf_append_char(&buf, ','))
             && sc_json_append_escaped(&buf, arg_names[i]);
        }
        w = w && sc_strbuf_append_staticstr(&buf, "],\"arg_values\":[");
        for (unsigned i = 0; w && i < arg_count; ++i) {
            w = (i == 0 || sc_strbuf_append_char(&buf, ','))
             && sc_json_append_escaped(&buf, arg_values[i]);
        }
        w = w && sc_strbuf_append_char(&buf, ']');
    }
    w = w && sc_strbuf_append_char(&buf, '}');
    if (!w) {
        free(buf.s);
        return false;
    }
    bool sent = sc_daemon_write_frame(ctx->socket, buf.s, buf.len, NULL, 0);
    free(buf.s);
    if (!sent) {
        LOGE("Client: could not send plugin request");
        return false;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(ctx->socket, &json, &doc)) {
        return false;
    }
    bool okv = false;
    bool ok = sc_json_get_bool(&json, "ok", &okv) && okv;
    if (ctx->opts->json) {
        if (ok) {
            char *raw = sc_json_get_raw(&json, "result");
            printf("%s\n", raw ? raw : "{}");
            free(raw);
        } else {
            char *err = sc_json_get_raw(&json, "error");
            printf("{\"error\":%s}\n", err ? err : "{\"code\":\"E_PLUGIN\","
                   "\"message\":\"plugin call failed\"}");
            free(err);
        }
        *ctx->json_printed = true;
    } else if (!ok) {
        LOGE("Client: request failed: %s", doc);
    }
    free(doc);
    return ok;
}

// Expand a leading "~/" to $HOME; returns a malloc'd path (caller frees).
static char *
expand_tilde(const char *path) {
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home && *home) {
            size_t hl = strlen(home);
            size_t tl = strlen(path + 1);
            char *out = malloc(hl + tl + 1);
            if (out) {
                memcpy(out, home, hl);
                memcpy(out + hl, path + 1, tl + 1);
            }
            return out;
        }
    }
    return strdup(path);
}

// Upload one local file to the daemon (op "upload" + raw payload) and return the
// daemon-side temp path it reports (malloc'd), or NULL on error.
static char *
do_upload(sc_socket socket, const char *local_path, int64_t id) {
    FILE *f = fopen(local_path, "rb");
    if (!f) {
        LOGE("Client: cannot open reference file \"%s\"", local_path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    uint8_t *data = malloc((size_t) sz + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(data, 1, (size_t) sz, f);
    fclose(f);

    const char *base = strrchr(local_path, '/');
    base = base ? base + 1 : local_path;

    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 128)) {
        free(data);
        return NULL;
    }
    char head[64];
    snprintf(head, sizeof(head),
             "{\"id\":%" PRId64 ",\"op\":\"upload\",\"name\":", id);
    char tail[48];
    snprintf(tail, sizeof(tail), ",\"payload_len\":%zu}", got);
    bool w = sc_strbuf_append_str(&buf, head)
          && sc_json_append_escaped(&buf, base)
          && sc_strbuf_append_str(&buf, tail);
    if (!w) {
        free(buf.s);
        free(data);
        return NULL;
    }
    bool sent = sc_daemon_write_frame(socket, buf.s, buf.len, data, got);
    free(buf.s);
    free(data);
    if (!sent) {
        LOGE("Client: could not upload reference file");
        return NULL;
    }

    struct sc_json json;
    char *doc;
    if (!client_read(socket, &json, &doc)) {
        return NULL;
    }
    char *path = NULL;
    if (response_ok(&json, doc)) {
        sc_json_get_string(&json, "path", &path);
    }
    free(doc);
    return path;
}

// --- add-on argument discovery (doc/addons.md) ---------------------------
//
// The daemon advertises each add-on's schema in its hello as an array of
// strings "<name>[|<arg>:<type>:<req>]...". The client parses that, then maps
// the raw unknown --name=value flags captured on the command line onto the
// right invocation: a flag is either a command (its value is the primary $1)
// or a declared argument of a command (sent on and exported as SC_ARG_<NAME>).

struct plugin_schema {
    char *name;
    char *arg_names[SC_MAX_ADDON_ARGS];
    bool arg_is_list[SC_MAX_ADDON_ARGS];
    bool arg_is_path[SC_MAX_ADDON_ARGS];
    bool arg_required[SC_MAX_ADDON_ARGS];
    unsigned arg_count;
};

// Parse one advertised keyed spec "<name>[|<key>=<value>]..." (mutated in
// place) into `out`. Only "arg=" fields are consumed here; result=/meta=/env=
// (used by the web UI) are ignored.
static void
parse_schema(char *spec, struct plugin_schema *out) {
    out->arg_count = 0;
    char *saveptr = NULL;
    out->name = strtok_r(spec, "|", &saveptr);
    char *field;
    while ((field = strtok_r(NULL, "|", &saveptr))) {
        if (strncmp(field, "arg=", 4) || out->arg_count >= SC_MAX_ADDON_ARGS) {
            continue;
        }
        char *sp2 = NULL;
        char *aname = strtok_r(field + 4, ":", &sp2);
        char *atype = strtok_r(NULL, ":", &sp2);
        char *areq = strtok_r(NULL, ":", &sp2);
        if (!aname || !*aname) {
            continue;
        }
        unsigned k = out->arg_count++;
        out->arg_names[k] = aname;
        out->arg_is_list[k] = atype && (!strcmp(atype, "list")
                                     || !strcmp(atype, "pathlist"));
        out->arg_is_path[k] = atype && (!strcmp(atype, "path")
                                     || !strcmp(atype, "pathlist"));
        out->arg_required[k] = areq && !strcmp(areq, "required");
    }
}

static const struct plugin_schema *
find_command(const struct plugin_schema *schemas, unsigned count,
             const char *flag) {
    for (unsigned i = 0; i < count; ++i) {
        if (schemas[i].name && !strcmp(schemas[i].name, flag)) {
            return &schemas[i];
        }
    }
    return NULL;
}

// If `flag` is a declared argument of `s`, return its index, else -1.
static int
find_arg(const struct plugin_schema *s, const char *flag) {
    for (unsigned i = 0; i < s->arg_count; ++i) {
        if (!strcmp(s->arg_names[i], flag)) {
            return (int) i;
        }
    }
    return -1;
}

static unsigned
count_flag(const struct scrcpy_options *opts, const char *flag) {
    unsigned c = 0;
    for (unsigned i = 0; i < opts->plugin_count; ++i) {
        if (flag && !strcmp(opts->plugin_names[i], flag)) {
            c++;
        }
    }
    return c;
}

// Collect argument `argname` from the raw flags. For a list arg, multiple
// occurrences are joined with '\n'. On present, sets *out to a malloc'd string
// (caller frees). Returns 1 if present, 0 if absent, -1 on error.
static int
collect_arg(const struct scrcpy_options *opts, const char *argname,
            bool is_list, char **out) {
    struct sc_strbuf b;
    bool have = false;
    for (unsigned i = 0; i < opts->plugin_count; ++i) {
        if (strcmp(opts->plugin_names[i], argname)) {
            continue;
        }
        if (!have) {
            if (!sc_strbuf_init(&b, 64)) {
                return -1;
            }
            have = true;
        } else if (!is_list) {
            LOGE("Client: --%s may be given only once", argname);
            free(b.s);
            return -1;
        } else if (!sc_strbuf_append_char(&b, '\n')) {
            free(b.s);
            return -1;
        }
        if (!sc_strbuf_append_str(&b, opts->plugin_args[i])) {
            free(b.s);
            return -1;
        }
    }
    if (!have) {
        return 0;
    }
    *out = b.s;
    return 1;
}

// Upload every '\n'-separated local path in `joined` to the daemon and return
// the daemon-side temp paths joined the same way (malloc'd), or NULL on error.
static char *
upload_paths(struct plugin_ctx *ctx, const char *joined) {
    char *copy = strdup(joined);
    if (!copy) {
        return NULL;
    }
    struct sc_strbuf out;
    if (!sc_strbuf_init(&out, 128)) {
        free(copy);
        return NULL;
    }
    bool ok = true, first = true;
    char *saveptr = NULL;
    for (char *p = strtok_r(copy, "\n", &saveptr); p;
         p = strtok_r(NULL, "\n", &saveptr)) {
        char *local = expand_tilde(p);
        char *remote = local ? do_upload(ctx->socket, local, (*ctx->id)++)
                             : NULL;
        free(local);
        if (!remote) {
            ok = false;
            break;
        }
        ok = (first || sc_strbuf_append_char(&out, '\n'))
          && sc_strbuf_append_str(&out, remote);
        free(remote);
        first = false;
        if (!ok) {
            break;
        }
    }
    free(copy);
    if (!ok) {
        free(out.s);
        return NULL;
    }
    return out.s;
}

// Group the raw --name=value flags into add-on invocations using the daemon's
// advertised schema, validate them, and run each. `specs` are the (mutable)
// spec strings from the hello; they are consumed (tokenized) here.
static bool
run_plugins(struct plugin_ctx *ctx, char **specs, unsigned spec_count) {
    const struct scrcpy_options *opts = ctx->opts;
    struct plugin_schema schemas[SC_MAX_ADDONS];
    unsigned n = spec_count < SC_MAX_ADDONS ? spec_count : SC_MAX_ADDONS;
    for (unsigned i = 0; i < n; ++i) {
        parse_schema(specs[i], &schemas[i]);
    }

    // Every raw flag must be a command, or a declared arg of an invoked command
    for (unsigned i = 0; i < opts->plugin_count; ++i) {
        const char *flag = opts->plugin_names[i];
        if (find_command(schemas, n, flag)) {
            continue;
        }
        const struct plugin_schema *owner = NULL;
        for (unsigned j = 0; j < n && !owner; ++j) {
            if (find_arg(&schemas[j], flag) >= 0) {
                owner = &schemas[j];
            }
        }
        if (!owner) {
            LOGE("Client: unknown option --%s (no add-on provides it; is the "
                 "daemon running with --add-on?)", flag);
            return false;
        }
        if (!count_flag(opts, owner->name)) {
            LOGE("Client: --%s is an argument of --%s, which was not given",
                 flag, owner->name);
            return false;
        }
    }

    // A command may be invoked at most once per client call (arg grouping)
    for (unsigned i = 0; i < n; ++i) {
        if (count_flag(opts, schemas[i].name) > 1) {
            LOGE("Client: --%s may be given only once", schemas[i].name);
            return false;
        }
    }

    // One invocation per command flag, in command order
    for (unsigned i = 0; i < opts->plugin_count; ++i) {
        const struct plugin_schema *s =
            find_command(schemas, n, opts->plugin_names[i]);
        if (!s) {
            continue;
        }

        const char *anames[SC_MAX_ADDON_ARGS];
        char *avalues[SC_MAX_ADDON_ARGS];
        unsigned ac = 0;
        bool ok = true;
        for (unsigned j = 0; j < s->arg_count; ++j) {
            char *val = NULL;
            int r = collect_arg(opts, s->arg_names[j], s->arg_is_list[j], &val);
            if (r < 0) {
                ok = false;
                break;
            }
            if (r == 0) {
                if (s->arg_required[j]) {
                    LOGE("Client: --%s requires --%s", s->name,
                         s->arg_names[j]);
                    ok = false;
                }
                continue;
            }
            // path/pathlist arg to a remote daemon: upload the bytes and pass
            // the daemon-side temp paths instead of the client's paths
            if (s->arg_is_path[j] && !ctx->local) {
                char *up = upload_paths(ctx, val);
                free(val);
                if (!up) {
                    ok = false;
                    break;
                }
                val = up;
            }
            anames[ac] = s->arg_names[j];
            avalues[ac] = val;
            ac++;
        }

        bool sent = ok && do_plugin(ctx, s->name, opts->plugin_args[i], anames,
                                    (const char *const *) avalues, ac,
                                    (*ctx->id)++);
        for (unsigned j = 0; j < ac; ++j) {
            free(avalues[j]);
        }
        if (!ok || !sent) {
            return false;
        }
    }
    return true;
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

// Parse a dotted-quad IPv4 literal into a host-order uint32 (like
// IPV4_LOCALHOST). Returns false if not a valid literal.
static bool
parse_ipv4(const char *s, uint32_t *out) {
    unsigned b[4];
    char extra;
    if (sscanf(s, "%u.%u.%u.%u%c", &b[0], &b[1], &b[2], &b[3], &extra) != 4) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (b[i] > 255) {
            return false;
        }
    }
    *out = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    return true;
}

// Resolve --daemon-host to an IPv4 address; sets *is_local for a loopback host
// (127.0.0.0/8, or "localhost"/default). Only IPv4 literals and "localhost"
// are accepted (hostname/DNS resolution is a future addition).
static bool
resolve_daemon_host(const char *host, uint32_t *addr, bool *is_local) {
    if (!host || !*host || !strcmp(host, "localhost")) {
        *addr = IPV4_LOCALHOST;
        *is_local = true;
        return true;
    }
    if (!parse_ipv4(host, addr)) {
        LOGE("Client: --daemon-host must be an IPv4 address or \"localhost\" "
             "(got \"%s\")", host);
        return false;
    }
    *is_local = (*addr & 0xFF000000u) == 0x7F000000u;
    return true;
}

enum scrcpy_exit_code
sc_client_run(const struct scrcpy_options *opts) {
    uint16_t port = opts->client_port;

    uint32_t addr;
    bool local;
    if (!resolve_daemon_host(opts->daemon_host, &addr, &local)) {
        return SCRCPY_EXIT_FAILURE;
    }
    const char *host_str = opts->daemon_host ? opts->daemon_host : "127.0.0.1";

    sc_socket socket = net_socket();
    if (socket == SC_SOCKET_NONE) {
        return SCRCPY_EXIT_FAILURE;
    }

    if (!net_connect(socket, addr, port)) {
        LOGE("Client: could not connect to daemon on %s:%" PRIu16
             " (is it running?)", host_str, port);
        net_close(socket);
        return SCRCPY_EXIT_DISCONNECTED;
    }

    net_set_tcp_nodelay(socket, true);

    enum scrcpy_exit_code ret = SCRCPY_EXIT_DISCONNECTED;
    bool json_printed = false; // whether a --json value was already emitted

    // Add-on argument schemas advertised in the hello (freed at end)
    char *plugin_specs[SC_MAX_ADDONS];
    unsigned plugin_spec_count = 0;

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
    // Discover add-on argument schemas so the plugin flags below can be grouped
    // into invocations (the strings are independent of hello_doc; freed at end)
    sc_json_get_string_array(&hello, "plugins", plugin_specs, SC_MAX_ADDONS,
                             &plugin_spec_count);
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

    if (opts->plugin_count) {
        struct plugin_ctx ctx = {
            .socket = socket,
            .opts = opts,
            .local = local,
            .json_printed = &json_printed,
            .id = &id,
        };
        if (!run_plugins(&ctx, plugin_specs, plugin_spec_count)) {
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
    // In --json mode always emit exactly one JSON value (unless a plugin
    // already printed its result/error object above).
    if (opts->json && !json_printed) {
        if (ret == SCRCPY_EXIT_SUCCESS) {
            printf("{\"ok\":true}\n");
        } else {
            printf("{\"error\":{\"code\":\"E_CLIENT\","
                   "\"message\":\"operation failed\"}}\n");
        }
    }
    for (unsigned i = 0; i < plugin_spec_count; ++i) {
        free(plugin_specs[i]);
    }
    net_close(socket);
    return ret;
}
