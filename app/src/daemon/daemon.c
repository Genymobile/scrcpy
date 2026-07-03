#include "daemon.h"

#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif

#include <libavutil/frame.h>

#include "control_exec.h"
#include "control_msg.h"
#include "controller.h"
#include "daemon/frame_keeper.h"
#include "daemon/protocol.h"
#include "daemon/registry.h"
#include "decoder.h"
#include "demuxer.h"
#include "screencap.h"
#include "server.h"
#include "util/log.h"
#include "util/net.h"
#include "util/rand.h"
#include "util/strbuf.h"
#include "util/thread.h"
#include "util/tick.h"

#define SC_DAEMON_MAX_CLIENTS 16
#define SC_DAEMON_SCREENCAP_DEADLINE SC_TICK_FROM_MS(2000)
// Poll interval for stop-signal checks in supervisor waits
#define SC_DAEMON_WAIT_TICK SC_TICK_FROM_MS(500)
#define SC_DAEMON_BACKOFF_MIN_MS 1000
#define SC_DAEMON_BACKOFF_MAX_MS 30000

enum sc_daemon_state {
    SC_DAEMON_STATE_CONNECTING,
    SC_DAEMON_STATE_READY,
    SC_DAEMON_STATE_RECONNECTING,
    SC_DAEMON_STATE_STOPPING,
};

static const char *
sc_daemon_state_str(enum sc_daemon_state state) {
    switch (state) {
        case SC_DAEMON_STATE_CONNECTING: return "connecting";
        case SC_DAEMON_STATE_READY: return "ready";
        case SC_DAEMON_STATE_RECONNECTING: return "reconnecting";
        case SC_DAEMON_STATE_STOPPING: return "stopping";
        default: return "unknown";
    }
}

struct sc_daemon_session {
    struct sc_server server;
    struct sc_demuxer demuxer;
    struct sc_decoder decoder;
    struct sc_controller controller;

    bool server_started;
    bool demuxer_started;
    bool controller_initialized;
    bool controller_started;

    // Guarded by daemon mutex
    bool connected;
    bool conn_failed;
    bool dead; // device lost after having been connected
};

struct sc_daemon;

struct sc_daemon_conn {
    struct sc_daemon *daemon;
    sc_socket socket;
    sc_thread thread;
    bool in_use;   // guarded by daemon mutex
    bool finished; // guarded by daemon mutex
};

struct sc_daemon {
    struct scrcpy_options *opts;
    sc_socket listen_socket;

    sc_mutex mutex;
    sc_cond cond; // signaled on state changes, stop, session events

    enum sc_daemon_state state;
    bool stop;
    unsigned in_flight; // requests using the session

    struct sc_daemon_session session;
    struct sc_frame_keeper keeper;

    sc_mutex clipboard_mutex; // serialize clipboard-based text injection

    char *serial;                // guarded by mutex, may be NULL
    char device_name[SC_DEVICE_NAME_FIELD_LENGTH]; // guarded by mutex

    sc_tick start_tick;

    sc_thread accept_thread;
    bool accept_thread_started;
    struct sc_daemon_conn conns[SC_DAEMON_MAX_CLIENTS];
};

static volatile sig_atomic_t g_stop_signal;

static void
sc_daemon_signal_handler(int sig) {
    (void) sig;
    g_stop_signal = 1;
}

static uint32_t
generate_scid(void) {
    struct sc_rand rand;
    sc_rand_init(&rand);
    // Only use 31 bits to avoid issues with signed values on the Java-side
    return sc_rand_u32(&rand) & 0x7FFFFFFF;
}

// Must be called with daemon->mutex locked
static void
sc_daemon_set_state_locked(struct sc_daemon *d, enum sc_daemon_state state) {
    d->state = state;
    sc_cond_broadcast(&d->cond);
}

static void
sc_daemon_update_registry(struct sc_daemon *d) {
    sc_mutex_lock(&d->mutex);
    const char *serial = d->serial;
    const char *state = sc_daemon_state_str(d->state);
    // Copy under lock, write outside
    char serial_copy[256];
    snprintf(serial_copy, sizeof(serial_copy), "%s", serial ? serial : "");
    char name_copy[SC_DEVICE_NAME_FIELD_LENGTH];
    snprintf(name_copy, sizeof(name_copy), "%s", d->device_name);
    sc_mutex_unlock(&d->mutex);

    sc_registry_write(d->opts->daemon_port, serial_copy, name_copy, state);
}

// ---- server/session callbacks ----

static void
sc_daemon_on_server_connection_failed(struct sc_server *server,
                                      void *userdata) {
    (void) server;
    struct sc_daemon *d = userdata;
    sc_mutex_lock(&d->mutex);
    d->session.conn_failed = true;
    sc_cond_broadcast(&d->cond);
    sc_mutex_unlock(&d->mutex);
}

static void
sc_daemon_on_server_connected(struct sc_server *server, void *userdata) {
    (void) server;
    struct sc_daemon *d = userdata;
    sc_mutex_lock(&d->mutex);
    d->session.connected = true;
    sc_cond_broadcast(&d->cond);
    sc_mutex_unlock(&d->mutex);
}

static void
sc_daemon_on_server_disconnected(struct sc_server *server, void *userdata) {
    (void) server;
    struct sc_daemon *d = userdata;
    sc_mutex_lock(&d->mutex);
    d->session.dead = true;
    sc_cond_broadcast(&d->cond);
    sc_mutex_unlock(&d->mutex);
}

static void
sc_daemon_on_demuxer_ended(struct sc_demuxer *demuxer,
                           enum sc_demuxer_status status, void *userdata) {
    (void) demuxer;
    (void) status;
    struct sc_daemon *d = userdata;
    sc_mutex_lock(&d->mutex);
    d->session.dead = true;
    sc_cond_broadcast(&d->cond);
    sc_mutex_unlock(&d->mutex);
}

static void
sc_daemon_on_controller_ended(struct sc_controller *controller, bool error,
                              void *userdata) {
    (void) controller;
    (void) error;
    struct sc_daemon *d = userdata;
    sc_mutex_lock(&d->mutex);
    d->session.dead = true;
    sc_cond_broadcast(&d->cond);
    sc_mutex_unlock(&d->mutex);
}

// ---- session lifecycle (called from the supervisor thread only) ----

static void
sc_daemon_session_stop(struct sc_daemon *d) {
    struct sc_daemon_session *s = &d->session;

    if (s->controller_started) {
        sc_controller_stop(&s->controller);
    }
    if (s->server_started) {
        // Shut down the sockets and kill the device-side process
        sc_server_stop(&s->server);
    }
    if (s->demuxer_started) {
        sc_demuxer_join(&s->demuxer);
    }
    if (s->controller_started) {
        sc_controller_join(&s->controller);
    }
    if (s->controller_initialized) {
        sc_controller_destroy(&s->controller);
    }
    if (s->server_started) {
        sc_server_join(&s->server);
        sc_server_destroy(&s->server);
    }

    s->server_started = false;
    s->demuxer_started = false;
    s->controller_initialized = false;
    s->controller_started = false;

    // Never serve a frame from a previous session
    sc_frame_keeper_reset(&d->keeper);

    sc_mutex_lock(&d->mutex);
    s->connected = false;
    s->conn_failed = false;
    s->dead = false;
    sc_mutex_unlock(&d->mutex);
}

static bool
sc_daemon_session_start(struct sc_daemon *d) {
    struct sc_daemon_session *s = &d->session;
    const struct scrcpy_options *options = d->opts;

    assert(!s->server_started);

    struct sc_server_params params = {
        .scid = generate_scid(),
        .req_serial = options->serial,
        .select_usb = options->select_usb,
        .select_tcpip = options->select_tcpip,
        .log_level = options->log_level,
        .video_codec = options->video_codec,
        .audio_codec = options->audio_codec,
        .video_source = options->video_source,
        .audio_source = options->audio_source,
        .camera_facing = options->camera_facing,
        .crop = options->crop,
        .port_range = options->port_range,
        .tunnel_host = options->tunnel_host,
        .tunnel_port = options->tunnel_port,
        .min_size_alignment = options->min_size_alignment,
        .max_size = options->max_size,
        .video_bit_rate = options->video_bit_rate,
        .audio_bit_rate = options->audio_bit_rate,
        .max_fps = options->max_fps,
        .angle = options->angle,
        .screen_off_timeout = options->screen_off_timeout,
        .capture_orientation = options->capture_orientation,
        .capture_orientation_lock = options->capture_orientation_lock,
        .control = options->control,
        .display_id = options->display_id,
        .new_display = options->new_display,
        .display_ime_policy = options->display_ime_policy,
        .video = options->video,
        .audio = false, // no audio in daemon mode (doc/daemon.md §6.1)
        .audio_dup = false,
        .show_touches = options->show_touches,
        .stay_awake = options->stay_awake,
        .video_codec_options = options->video_codec_options,
        .audio_codec_options = options->audio_codec_options,
        .video_encoder = options->video_encoder,
        .audio_encoder = options->audio_encoder,
        .camera_id = options->camera_id,
        .camera_size = options->camera_size,
        .camera_ar = options->camera_ar,
        .camera_fps = options->camera_fps,
        .force_adb_forward = options->force_adb_forward,
        .power_off_on_close = options->power_off_on_close,
        .clipboard_autosync = false, // no computer clipboard in daemon mode
        .downsize_on_error = options->downsize_on_error,
        .tcpip = options->tcpip,
        .tcpip_dst = options->tcpip_dst,
        .cleanup = options->cleanup,
        .power_on = options->power_on,
        .kill_adb_on_close = false, // adb is shared with future reconnects
        .camera_high_speed = options->camera_high_speed,
        .camera_torch = options->camera_torch,
        .camera_zoom = options->camera_zoom,
        .vd_destroy_content = options->vd_destroy_content,
        .vd_system_decorations = options->vd_system_decorations,
        .keep_active = options->keep_active,
        .flex_display = false,
        .list = 0,
    };

    static const struct sc_server_callbacks cbs = {
        .on_connection_failed = sc_daemon_on_server_connection_failed,
        .on_connected = sc_daemon_on_server_connected,
        .on_disconnected = sc_daemon_on_server_disconnected,
    };

    if (!sc_server_init(&s->server, &params, &cbs, d)) {
        return false;
    }

    if (!sc_server_start(&s->server)) {
        sc_server_destroy(&s->server);
        return false;
    }
    s->server_started = true;

    // Wait for connection result (or stop request)
    sc_mutex_lock(&d->mutex);
    while (!s->connected && !s->conn_failed && !d->stop) {
        if (g_stop_signal) {
            d->stop = true;
            break;
        }
        sc_cond_timedwait(&d->cond, &d->mutex,
                          sc_tick_now() + SC_DAEMON_WAIT_TICK);
    }
    bool connected = s->connected && !d->stop;
    sc_mutex_unlock(&d->mutex);

    if (!connected) {
        sc_daemon_session_stop(d);
        return false;
    }

    if (options->video) {
        static const struct sc_demuxer_callbacks demuxer_cbs = {
            .on_ended = sc_daemon_on_demuxer_ended,
        };
        sc_demuxer_init(&s->demuxer, "video", s->server.video_socket,
                        &demuxer_cbs, d);
        sc_decoder_init(&s->decoder, "video");
        sc_packet_source_add_sink(&s->demuxer.packet_source,
                                  &s->decoder.packet_sink);
        sc_frame_source_add_sink(&s->decoder.frame_source,
                                 &d->keeper.frame_sink);
        if (!sc_demuxer_start(&s->demuxer)) {
            sc_daemon_session_stop(d);
            return false;
        }
        s->demuxer_started = true;
    }

    if (options->control) {
        static const struct sc_controller_callbacks controller_cbs = {
            .on_ended = sc_daemon_on_controller_ended,
        };
        if (!sc_controller_init(&s->controller, s->server.control_socket,
                                &controller_cbs, d)) {
            sc_daemon_session_stop(d);
            return false;
        }
        s->controller_initialized = true;
        sc_controller_configure(&s->controller, NULL, NULL);
        if (!sc_controller_start(&s->controller)) {
            sc_daemon_session_stop(d);
            return false;
        }
        s->controller_started = true;
    }

    // Record device identity for hello/status/registry
    sc_mutex_lock(&d->mutex);
    free(d->serial);
    d->serial = s->server.serial ? strdup(s->server.serial) : NULL;
    snprintf(d->device_name, sizeof(d->device_name), "%s",
             s->server.info.device_name);
    sc_mutex_unlock(&d->mutex);

    return true;
}

// ---- request handling (called from connection threads) ----

static bool
send_frame(sc_socket socket, struct sc_strbuf *buf, const uint8_t *payload,
           size_t payload_len) {
    bool ok = sc_daemon_write_frame(socket, buf->s, buf->len, payload,
                                    payload_len);
    free(buf->s);
    return ok;
}

static bool
send_error(sc_socket socket, int64_t id, const char *code, const char *msg) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 128)) {
        return false;
    }

    // Large enough for the template + a 20-char int64 + any error code
    char head[128];
    snprintf(head, sizeof(head), "{\"id\":%" PRId64 ",\"ok\":false,"
                                 "\"error\":{\"code\":\"%s\",\"message\":",
             id, code);

    bool w = sc_strbuf_append_str(&buf, head)
          && sc_json_append_escaped(&buf, msg)
          && sc_strbuf_append_staticstr(&buf, "}}");
    if (!w) {
        free(buf.s);
        return false;
    }

    return send_frame(socket, &buf, NULL, 0);
}

// Send {"id":<id>,"ok":true[,<extra>]} (+ optional payload); `extra` must be
// valid inner JSON without braces, or NULL
static bool
send_ok(sc_socket socket, int64_t id, const char *extra,
        const uint8_t *payload, size_t payload_len) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 128)) {
        return false;
    }

    char head[48];
    snprintf(head, sizeof(head), "{\"id\":%" PRId64 ",\"ok\":true", id);

    bool w = sc_strbuf_append_str(&buf, head);
    if (w && extra) {
        w = sc_strbuf_append_char(&buf, ',')
         && sc_strbuf_append_str(&buf, extra);
    }
    w = w && sc_strbuf_append_char(&buf, '}');
    if (!w) {
        free(buf.s);
        return false;
    }

    return send_frame(socket, &buf, payload, payload_len);
}

// Append common identity/state fields (without braces) to `buf`.
// Takes and releases the daemon mutex.
static bool
append_status_fields(struct sc_daemon *d, struct sc_strbuf *buf) {
    sc_mutex_lock(&d->mutex);
    enum sc_daemon_state state = d->state;
    char serial[256];
    snprintf(serial, sizeof(serial), "%s", d->serial ? d->serial : "");
    char device_name[SC_DEVICE_NAME_FIELD_LENGTH];
    snprintf(device_name, sizeof(device_name), "%s", d->device_name);
    sc_mutex_unlock(&d->mutex);

    char head[128];
    snprintf(head, sizeof(head), "\"protocol\":%d,\"app\":\"scrcpy-auto\","
                                 "\"version\":\"%s\",\"state\":\"%s\",",
             SC_DAEMON_PROTOCOL_VERSION, SCRCPY_VERSION,
             sc_daemon_state_str(state));

    return sc_strbuf_append_str(buf, head)
        && sc_strbuf_append_staticstr(buf, "\"serial\":")
        && sc_json_append_escaped(buf, serial)
        && sc_strbuf_append_staticstr(buf, ",\"device_name\":")
        && sc_json_append_escaped(buf, device_name);
}

static bool
send_hello(struct sc_daemon *d, sc_socket socket) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        return false;
    }

    char pid[48];
    snprintf(pid, sizeof(pid), ",\"pid\":%lu}",
             (unsigned long)
#ifdef _WIN32
             GetCurrentProcessId()
#else
             getpid()
#endif
    );

    bool w = sc_strbuf_append_staticstr(&buf, "{\"event\":\"hello\",")
          && append_status_fields(d, &buf)
          && sc_strbuf_append_str(&buf, pid);
    if (!w) {
        free(buf.s);
        return false;
    }

    return send_frame(socket, &buf, NULL, 0);
}

// Acquire the session for a request. On success, in_flight was incremented
// and the session is READY. Returns false after sending E_NOT_READY.
static bool
acquire_session(struct sc_daemon *d, sc_socket socket, int64_t id) {
    sc_mutex_lock(&d->mutex);
    if (d->state != SC_DAEMON_STATE_READY || d->stop) {
        enum sc_daemon_state state = d->state;
        sc_mutex_unlock(&d->mutex);
        char msg[64];
        snprintf(msg, sizeof(msg), "session not ready (state: %s)",
                 sc_daemon_state_str(state));
        send_error(socket, id, "E_NOT_READY", msg);
        return false;
    }
    ++d->in_flight;
    sc_mutex_unlock(&d->mutex);
    return true;
}

static void
release_session(struct sc_daemon *d) {
    sc_mutex_lock(&d->mutex);
    assert(d->in_flight);
    --d->in_flight;
    sc_cond_broadcast(&d->cond);
    sc_mutex_unlock(&d->mutex);
}

static void
handle_status(struct sc_daemon *d, sc_socket socket, int64_t id) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        return;
    }

    if (!append_status_fields(d, &buf)) {
        free(buf.s);
        return;
    }

    sc_tick now = sc_tick_now();
    sc_tick last_frame = sc_frame_keeper_last_tick(&d->keeper);
    int64_t frame_age_ms = last_frame ? SC_TICK_TO_MS(now - last_frame) : -1;

    char tail[96];
    snprintf(tail, sizeof(tail),
             ",\"uptime_ms\":%" PRId64 ",\"last_frame_age_ms\":%" PRId64,
             (int64_t) SC_TICK_TO_MS(now - d->start_tick), frame_age_ms);
    if (!sc_strbuf_append_str(&buf, tail)) {
        free(buf.s);
        return;
    }

    send_ok(socket, id, buf.s, NULL, 0);
    free(buf.s);
}

static void
handle_screencap(struct sc_daemon *d, sc_socket socket, int64_t id,
                 const struct sc_json *json) {
    if (!d->opts->video) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "video is disabled (--no-video)");
        return;
    }

    char *format = NULL;
    if (sc_json_get_string(json, "format", &format)) {
        bool ok = !strcmp(format, "png");
        free(format);
        if (!ok) {
            send_error(socket, id, "E_BAD_REQUEST", "unsupported format");
            return;
        }
    }

    if (!acquire_session(d, socket, id)) {
        return;
    }

    sc_tick now = sc_tick_now();
    sc_tick min_tick = 0;
    sc_tick deadline = now + SC_DAEMON_SCREENCAP_DEADLINE;

    int64_t max_age_ms;
    if (sc_json_get_int64(json, "max_age_ms", &max_age_ms)
            && max_age_ms >= 0) {
        // Clamp to avoid int64 overflow in SC_TICK_FROM_MS and negative
        // min_tick (which would bypass the wait-for-first-frame loop)
        int64_t max_age_capped = MIN(max_age_ms, (int64_t) 1 << 40); // ~35y ms
        min_tick = now - SC_TICK_FROM_MS(max_age_capped);
        if (min_tick < 0) {
            min_tick = 0;
        }
        if (sc_frame_keeper_last_tick(&d->keeper) < min_tick
                && d->session.controller_started) {
            // Ask the device encoder for a fresh keyframe
            struct sc_control_msg msg = {
                .type = SC_CONTROL_MSG_TYPE_RESET_VIDEO,
            };
            if (!sc_controller_push_msg(&d->session.controller, &msg)) {
                LOGW("Could not request video reset");
            }
        }
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        LOG_OOM();
        release_session(d);
        send_error(socket, id, "E_INTERNAL", "out of memory");
        return;
    }

    sc_tick age;
    bool ok = sc_frame_keeper_get_since(&d->keeper, frame, min_tick, deadline,
                                        &age);
    if (!ok) {
        av_frame_free(&frame);
        release_session(d);
        send_error(socket, id, "E_TIMEOUT", "no frame available");
        return;
    }

    uint8_t *png;
    size_t png_size;
    ok = sc_frame_to_png(frame, &png, &png_size);
    int width = frame->width;
    int height = frame->height;
    av_frame_free(&frame);
    release_session(d);

    if (!ok) {
        send_error(socket, id, "E_INTERNAL", "PNG encoding failed");
        return;
    }

    char extra[128];
    snprintf(extra, sizeof(extra),
             "\"width\":%d,\"height\":%d,\"frame_age_ms\":%" PRId64
             ",\"payload_len\":%zu",
             width, height, (int64_t) SC_TICK_TO_MS(age), png_size);
    send_ok(socket, id, extra, png, png_size);
    free(png);
}

static void
handle_control(struct sc_daemon *d, sc_socket socket, int64_t id,
               const struct sc_json *json, unsigned conn_index) {
    if (!d->opts->control) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "control is disabled (--no-control)");
        return;
    }

    char *cmds[SC_MAX_CONTROL_CMDS];
    unsigned count;
    if (!sc_json_get_string_array(json, "cmds", cmds, SC_MAX_CONTROL_CMDS,
                                  &count) || !count) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "missing or invalid \"cmds\" array");
        return;
    }

    // Reject malformed commands up front with an explicit client-visible
    // error (execution failures would only give a generic message)
    if (!sc_control_exec_check((const char *const *) cmds, count)) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "invalid control command syntax; expected: "
                   "\"click <x> <y> [duration_ms]\", "
                   "\"swipe <x1> <y1> <x2> <y2> [duration_ms]\", "
                   "\"input <text>\", \"sleep <ms>\", steps joined by \"&&\"");
        goto free_cmds;
    }

    if (!acquire_session(d, socket, id)) {
        goto free_cmds;
    }

    // With video captured, the device-side PositionMapper IGNORES touch
    // events whose screen_size does not match the video size, so send the
    // actual video size and interpret coordinates in video (screenshot)
    // space; without video, the device uses raw coordinates
    struct sc_size screen_size = {UINT16_MAX, UINT16_MAX};
    if (d->opts->video
            && !sc_frame_keeper_wait_size(&d->keeper,
                                          sc_tick_now()
                                              + SC_DAEMON_SCREENCAP_DEADLINE,
                                          &screen_size)) {
        release_session(d);
        send_error(socket, id, "E_NOT_READY", "no video frame received yet");
        goto free_cmds;
    }

    // Each connection thread executes one request at a time, so deriving the
    // range from the connection slot guarantees concurrent requests use
    // disjoint pointer ids (doc/daemon.md §9.5): at most
    // 1 + 16*100 + 99 = 1700, far below the reserved values
    // (SC_POINTER_ID_MOUSE, ...) at the top of the uint64 range
    uint64_t pointer_base = 1 + (uint64_t) conn_index * SC_MAX_CONTROL_CMDS;

    // Serialize clipboard-based text injection between concurrent requests
    bool needs_clipboard = false;
    for (unsigned i = 0; i < count; ++i) {
        if (strstr(cmds[i], "input ")) {
            needs_clipboard = true;
            break;
        }
    }

    if (needs_clipboard) {
        sc_mutex_lock(&d->clipboard_mutex);
    }

    bool ok = sc_control_exec_run(&d->session.controller, screen_size,
                                  (const char *const *) cmds, count,
                                  pointer_base);

    if (needs_clipboard) {
        sc_mutex_unlock(&d->clipboard_mutex);
    }

    release_session(d);

    if (ok) {
        send_ok(socket, id, NULL, NULL, 0);
    } else {
        send_error(socket, id, "E_INTERNAL", "control commands failed");
    }

free_cmds:
    for (unsigned i = 0; i < count; ++i) {
        free(cmds[i]);
    }
}

// Handle one request; returns false if the connection must be closed
static bool
handle_request(struct sc_daemon *d, sc_socket socket, const char *json_str,
               size_t json_len, unsigned conn_index) {
    struct sc_json json;
    if (!sc_json_parse(&json, json_str, json_len)) {
        send_error(socket, 0, "E_BAD_REQUEST", "invalid JSON");
        return false;
    }

    int64_t id = 0;
    sc_json_get_int64(&json, "id", &id);

    char *op;
    if (!sc_json_get_string(&json, "op", &op)) {
        send_error(socket, id, "E_BAD_REQUEST", "missing \"op\"");
        return true;
    }

    bool keep_open = true;

    if (!strcmp(op, "ping")) {
        send_ok(socket, id, NULL, NULL, 0);
    } else if (!strcmp(op, "status")) {
        handle_status(d, socket, id);
    } else if (!strcmp(op, "screencap")) {
        handle_screencap(d, socket, id, &json);
    } else if (!strcmp(op, "control")) {
        handle_control(d, socket, id, &json, conn_index);
    } else if (!strcmp(op, "shutdown")) {
        send_ok(socket, id, NULL, NULL, 0);
        LOGI("Daemon: shutdown requested by client");
        sc_mutex_lock(&d->mutex);
        d->stop = true;
        sc_cond_broadcast(&d->cond);
        sc_mutex_unlock(&d->mutex);
        keep_open = false;
    } else {
        send_error(socket, id, "E_BAD_REQUEST", "unknown op");
    }

    free(op);
    return keep_open;
}

// ---- IPC threads ----

// Defined in the supervisor section below
static bool
sc_daemon_interruptible_sleep(struct sc_daemon *d, sc_tick duration);

static int
run_connection(void *data) {
    struct sc_daemon_conn *conn = data;
    struct sc_daemon *d = conn->daemon;
    sc_socket socket = conn->socket;
    unsigned conn_index = (unsigned) (conn - d->conns);

    if (send_hello(d, socket)) {
        for (;;) {
            char *json;
            size_t len;
            if (!sc_daemon_read_json(socket, &json, &len)) {
                break;
            }
            bool keep_open = handle_request(d, socket, json, len, conn_index);
            free(json);
            if (!keep_open) {
                break;
            }
        }
    }

    net_close(socket);

    sc_mutex_lock(&d->mutex);
    conn->finished = true;
    sc_mutex_unlock(&d->mutex);

    return 0;
}

// Must be called with daemon->mutex locked
static struct sc_daemon_conn *
find_conn_slot_locked(struct sc_daemon *d) {
    for (unsigned i = 0; i < SC_DAEMON_MAX_CLIENTS; ++i) {
        struct sc_daemon_conn *conn = &d->conns[i];
        if (conn->in_use && conn->finished) {
            sc_thread_join(&conn->thread, NULL);
            conn->in_use = false;
        }
        if (!conn->in_use) {
            return conn;
        }
    }
    return NULL;
}

static int
run_accept(void *data) {
    struct sc_daemon *d = data;

    for (;;) {
        sc_socket client = net_accept(d->listen_socket);
        if (client == SC_SOCKET_NONE) {
            // Either interrupted for stopping, or a transient accept error
            // (connection aborted in the backlog, fd exhaustion, ...):
            // net_accept() does not distinguish them, so check the stop flag
            // and keep accepting otherwise
            if (sc_daemon_interruptible_sleep(d, SC_TICK_FROM_MS(100))) {
                break;
            }
            LOGW("Daemon: accept failed, retrying");
            continue;
        }

        net_set_tcp_nodelay(client, true);

        sc_mutex_lock(&d->mutex);
        if (d->stop) {
            sc_mutex_unlock(&d->mutex);
            net_close(client);
            break;
        }

        struct sc_daemon_conn *conn = find_conn_slot_locked(d);
        if (!conn) {
            sc_mutex_unlock(&d->mutex);
            LOGW("Daemon: too many concurrent clients, rejecting");
            send_error(client, 0, "E_BUSY", "too many concurrent clients");
            net_close(client);
            continue;
        }

        conn->daemon = d;
        conn->socket = client;
        conn->in_use = true;
        conn->finished = false;
        if (!sc_thread_create(&conn->thread, run_connection, "scrcpy-ipc",
                              conn)) {
            conn->in_use = false;
            sc_mutex_unlock(&d->mutex);
            LOGE("Daemon: could not create connection thread");
            net_close(client);
            continue;
        }
        sc_mutex_unlock(&d->mutex);
    }

    return 0;
}

// ---- supervisor ----

// Sleep up to `duration`, waking early on stop; returns true if stopped
static bool
sc_daemon_interruptible_sleep(struct sc_daemon *d, sc_tick duration) {
    sc_tick deadline = sc_tick_now() + duration;
    sc_mutex_lock(&d->mutex);
    while (!d->stop && sc_tick_now() < deadline) {
        if (g_stop_signal) {
            d->stop = true;
            break;
        }
        sc_tick tick_deadline = sc_tick_now() + SC_DAEMON_WAIT_TICK;
        if (tick_deadline > deadline) {
            tick_deadline = deadline;
        }
        sc_cond_timedwait(&d->cond, &d->mutex, tick_deadline);
    }
    bool stopped = d->stop;
    sc_mutex_unlock(&d->mutex);
    return stopped;
}

// Wait while READY, until the session dies or stop is requested
static void
sc_daemon_wait_session_end(struct sc_daemon *d) {
    sc_mutex_lock(&d->mutex);
    while (!d->stop && !d->session.dead) {
        if (g_stop_signal) {
            d->stop = true;
            break;
        }
        sc_cond_timedwait(&d->cond, &d->mutex,
                          sc_tick_now() + SC_DAEMON_WAIT_TICK);
    }
    sc_mutex_unlock(&d->mutex);
}

// Block new requests and wait for in-flight requests to complete
static void
sc_daemon_drain_requests(struct sc_daemon *d, enum sc_daemon_state state) {
    sc_mutex_lock(&d->mutex);
    sc_daemon_set_state_locked(d, state);
    while (d->in_flight) {
        if (g_stop_signal) {
            // Cannot abort in-flight requests, but record the stop request
            // so that the supervisor exits once they complete
            d->stop = true;
        }
        sc_cond_timedwait(&d->cond, &d->mutex,
                          sc_tick_now() + SC_DAEMON_WAIT_TICK);
    }
    sc_mutex_unlock(&d->mutex);
}

enum scrcpy_exit_code
sc_daemon_run(struct scrcpy_options *opts) {
    enum scrcpy_exit_code ret = SCRCPY_EXIT_FAILURE;

    struct sc_daemon *d = calloc(1, sizeof(*d));
    if (!d) {
        LOG_OOM();
        return SCRCPY_EXIT_FAILURE;
    }

    d->opts = opts;
    d->state = SC_DAEMON_STATE_CONNECTING;
    d->start_tick = sc_tick_now();

    bool mutex_ok = false;
    bool cond_ok = false;
    bool clip_ok = false;
    bool keeper_ok = false;
    bool listening = false;
    bool registry_written = false;

    if (!sc_mutex_init(&d->mutex)) {
        goto end;
    }
    mutex_ok = true;
    if (!sc_cond_init(&d->cond)) {
        goto end;
    }
    cond_ok = true;
    if (!sc_mutex_init(&d->clipboard_mutex)) {
        goto end;
    }
    clip_ok = true;
    if (!sc_frame_keeper_init(&d->keeper)) {
        goto end;
    }
    keeper_ok = true;

    // Bind first: this is the "already running?" check (doc/daemon.md §6.1)
    d->listen_socket = net_socket();
    if (d->listen_socket == SC_SOCKET_NONE) {
        goto end;
    }
    if (!net_listen(d->listen_socket, IPV4_LOCALHOST, opts->daemon_port, 4)) {
        LOGE("Daemon: could not listen on 127.0.0.1:%" PRIu16
             " (already in use?)", opts->daemon_port);
        net_close(d->listen_socket);
        goto end;
    }
    listening = true;

    signal(SIGINT, sc_daemon_signal_handler);
    signal(SIGTERM, sc_daemon_signal_handler);

    registry_written = sc_registry_write(opts->daemon_port, opts->serial, "",
                                         "starting");

    if (!sc_thread_create(&d->accept_thread, run_accept, "scrcpy-accept", d)) {
        LOGE("Daemon: could not create accept thread");
        goto end;
    }
    d->accept_thread_started = true;

    LOGI("Daemon: listening on 127.0.0.1:%" PRIu16, opts->daemon_port);

    unsigned failures = 0;
    unsigned backoff_ms = SC_DAEMON_BACKOFF_MIN_MS;

    for (;;) {
        sc_mutex_lock(&d->mutex);
        bool stop = d->stop || g_stop_signal;
        d->stop = stop;
        if (!stop) {
            sc_daemon_set_state_locked(d, failures
                                              ? SC_DAEMON_STATE_RECONNECTING
                                              : SC_DAEMON_STATE_CONNECTING);
        }
        sc_mutex_unlock(&d->mutex);
        if (stop) {
            ret = SCRCPY_EXIT_SUCCESS;
            break;
        }
        sc_daemon_update_registry(d);

        if (!sc_daemon_session_start(d)) {
            sc_mutex_lock(&d->mutex);
            bool stopped = d->stop || g_stop_signal;
            sc_mutex_unlock(&d->mutex);
            if (stopped) {
                ret = SCRCPY_EXIT_SUCCESS;
                break;
            }

            ++failures;
            if (opts->daemon_reconnect_max
                    && failures > opts->daemon_reconnect_max) {
                LOGE("Daemon: could not connect to the device after %u "
                     "attempts, giving up", failures);
                break;
            }
            LOGW("Daemon: connection failed, retrying in %u ms", backoff_ms);
            if (sc_daemon_interruptible_sleep(d,
                                              SC_TICK_FROM_MS(backoff_ms))) {
                ret = SCRCPY_EXIT_SUCCESS;
                break;
            }
            backoff_ms = MIN(backoff_ms * 2, SC_DAEMON_BACKOFF_MAX_MS);
            continue;
        }

        failures = 0;
        backoff_ms = SC_DAEMON_BACKOFF_MIN_MS;

        sc_mutex_lock(&d->mutex);
        sc_daemon_set_state_locked(d, SC_DAEMON_STATE_READY);
        sc_mutex_unlock(&d->mutex);
        sc_daemon_update_registry(d);
        LOGI("Daemon: device session ready");

        sc_daemon_wait_session_end(d);

        sc_mutex_lock(&d->mutex);
        bool stopping = d->stop;
        sc_mutex_unlock(&d->mutex);

        sc_daemon_drain_requests(d, stopping ? SC_DAEMON_STATE_STOPPING
                                             : SC_DAEMON_STATE_RECONNECTING);
        sc_daemon_session_stop(d);

        if (stopping) {
            ret = SCRCPY_EXIT_SUCCESS;
            break;
        }

        LOGW("Daemon: device disconnected");
        if (opts->daemon_reconnect_none) {
            LOGI("Daemon: reconnection disabled, exiting");
            ret = SCRCPY_EXIT_FAILURE;
            break;
        }
        ++failures; // report "reconnecting" state on next iteration
    }

    // Shutdown
    sc_mutex_lock(&d->mutex);
    d->stop = true;
    sc_daemon_set_state_locked(d, SC_DAEMON_STATE_STOPPING);
    sc_mutex_unlock(&d->mutex);

end:
    if (listening) {
        net_interrupt(d->listen_socket);
    }
    if (d->accept_thread_started) {
        sc_thread_join(&d->accept_thread, NULL);
    }

    // Interrupt and join client connections
    if (mutex_ok) {
        sc_mutex_lock(&d->mutex);
        for (unsigned i = 0; i < SC_DAEMON_MAX_CLIENTS; ++i) {
            struct sc_daemon_conn *conn = &d->conns[i];
            if (conn->in_use && !conn->finished) {
                net_interrupt(conn->socket);
            }
        }
        sc_mutex_unlock(&d->mutex);
        for (unsigned i = 0; i < SC_DAEMON_MAX_CLIENTS; ++i) {
            struct sc_daemon_conn *conn = &d->conns[i];
            if (conn->in_use) {
                sc_thread_join(&conn->thread, NULL);
                conn->in_use = false;
            }
        }
    }

    if (listening) {
        net_close(d->listen_socket);
    }
    if (registry_written) {
        sc_registry_remove(opts->daemon_port);
    }
    if (keeper_ok) {
        sc_frame_keeper_destroy(&d->keeper);
    }
    if (clip_ok) {
        sc_mutex_destroy(&d->clipboard_mutex);
    }
    if (cond_ok) {
        sc_cond_destroy(&d->cond);
    }
    if (mutex_ok) {
        sc_mutex_destroy(&d->mutex);
    }
    free(d->serial);
    free(d);

    return ret;
}
