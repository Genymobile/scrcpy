#include "daemon.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdatomic.h>
#ifndef _WIN32
# include <fcntl.h>
# include <sys/wait.h>
# include <time.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif

#include <libavutil/frame.h>

#include "adb/adb.h"
#include "control_exec.h"
#include "control_msg.h"
#include "controller.h"
#include "coords.h"
#include "daemon/addon.h"
#include "daemon/broadcaster.h"
#include "daemon/codec.h"
#include "daemon/clip_buffer.h"
#include "daemon/frame_keeper.h"
#include "daemon/plugin_event.h"
#include "daemon/protocol.h"
#include "daemon/registry.h"
#include "daemon/report.h"
#include "daemon/touch_report.h"
#include "util/file.h"
#include "util/process.h"
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
#define SC_MAX_SERVICES 8               // adopted long-running service add-ons
#define SC_SERVICE_READY_TIMEOUT_MS 15000 // wait for a service to report ready
#define SC_SERVICE_POLL_MS 25           // service readiness poll interval
#define SC_SERVICE_TERM_GRACE_MS 2000   // SIGTERM grace before SIGKILL
#define SC_SERVICE_KILL_WAIT_MS 2000    // bounded wait after forced termination
// Files a single client connection may upload before they are cleaned up
#define SC_DAEMON_MAX_UPLOADS 64
#define SC_DAEMON_SCREENCAP_DEADLINE SC_TICK_FROM_MS(2000)
#define SC_DAEMON_FIRST_FRAME_DEADLINE SC_TICK_FROM_SEC(15)
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
    sc_pid plugin_pid; // running add-on child, or SC_PROCESS_NONE (mutex)
    char *uploads[SC_DAEMON_MAX_UPLOADS]; // temp files to unlink on close (mutex)
    unsigned upload_count; // guarded by daemon mutex
    // Accessed only by this connection's thread; reset when the slot is reused.
    struct sc_touch_report touch_report;
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
    struct sc_broadcaster broadcaster; // encoded-video push to web clients
    struct sc_clip_buffer clips; // encoded-stream spool for clip extraction

    // Test-report platform (DESIGN-test-report.md)
    struct sc_report report;
    bool report_active;      // --auto-test-report was given
    atomic_bool report_initialized; // report dir/log created (once)
    atomic_bool report_recording;   // recorder running this session

    struct sc_addons addons; // loaded plugins (doc/addons.md)
    unsigned plugin_asset_counter; // guarded by mutex

    // Adopted long-running "service" add-ons (doc/addons.md): started on demand,
    // kept running past their response, terminated on daemon shutdown. Guarded
    // by `mutex`.
    struct sc_service_proc {
        char *name;
        sc_pid pid;
    } services[SC_MAX_SERVICES];
    unsigned service_count;

    sc_mutex clipboard_mutex; // serialize clipboard-based text injection

    char *serial;                // guarded by mutex, may be NULL
    char real_device_udid[256];  // resolved once on the first device session
    bool real_device_udid_initialized;
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
    struct sc_daemon *d = userdata;
    if (status == SC_DEMUXER_STATUS_ERROR && d->report_initialized) {
        sc_report_mark_failed(&d->report);
    }
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

    // Finalize the recording (reads the keeper video-time, so before reset)
    if (d->report_recording) {
        sc_report_stop_recording(&d->report);
        d->report_recording = false;
    }

    // Never serve a frame from a previous session
    sc_frame_keeper_reset(&d->keeper);

    sc_mutex_lock(&d->mutex);
    s->connected = false;
    s->conn_failed = false;
    s->dead = false;
    sc_mutex_unlock(&d->mutex);
}

static bool
sc_daemon_wait_first_report_frame(struct sc_daemon *d) {
    sc_tick deadline = sc_tick_now() + SC_DAEMON_FIRST_FRAME_DEADLINE;
    for (;;) {
        sc_tick now = sc_tick_now();
        if (now >= deadline) {
            return false;
        }
        sc_tick slice = now + SC_DAEMON_WAIT_TICK;
        if (slice > deadline) {
            slice = deadline;
        }

        struct sc_size first_size;
        if (sc_frame_keeper_wait_size(&d->keeper, slice, &first_size)) {
            return true;
        }

        sc_mutex_lock(&d->mutex);
        bool interrupted = d->stop || d->session.dead || g_stop_signal;
        if (g_stop_signal) {
            d->stop = true;
        }
        sc_mutex_unlock(&d->mutex);
        if (interrupted) {
            return false;
        }
    }
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
        .flex_display = options->flex_display,
        .ignore_video_encoder_constraints =
            options->ignore_video_encoder_constraints,
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

    // Resolve the physical device serial exactly once for this daemon. For
    // TCP/IP devices, s->server.serial is the transport address, so expose both
    // identities to add-ons without requiring every invocation to shell out to
    // adb. If get-serialno fails, retain the transport serial as the fallback.
    if (!d->real_device_udid_initialized) {
        char *real_device_udid =
            sc_adb_get_serialno(&s->server.intr, s->server.serial,
                                SC_ADB_SILENT);
        snprintf(d->real_device_udid, sizeof(d->real_device_udid), "%s",
                 real_device_udid ? real_device_udid
                                  : s->server.serial ? s->server.serial : "");
        d->real_device_udid_initialized = true;
        free(real_device_udid);
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
        // Forward the encoded stream to web subscribers (second packet sink)
        sc_packet_source_add_sink(&s->demuxer.packet_source,
                                  &d->broadcaster.packet_sink);

        // Spool the encoded stream for clip extraction (third packet sink)
        sc_packet_source_add_sink(&s->demuxer.packet_source,
                                  &d->clips.packet_sink);

        // Test report: record the encoded stream (fourth packet sink)
        if (d->report_active) {
            if (!d->report_initialized) {
                if (!sc_report_init(&d->report, options->auto_test_report,
                                    &d->keeper, &d->clips, s->server.serial,
                                    s->server.info.device_name,
                                    options->video_codec)) {
                    sc_daemon_session_stop(d);
                    return false;
                }
                d->report_initialized = true;
            }
            if (!sc_report_start_recording(&d->report, true,
                                           SC_ORIENTATION_0)) {
                LOGE("Test report: could not start recording");
                sc_report_stop_recording(&d->report);
                sc_daemon_session_stop(d);
                return false;
            }
            d->report_recording = true;
            sc_packet_source_add_sink(&s->demuxer.packet_source,
                                      sc_report_video_sink(&d->report));
        }

        if (!sc_demuxer_start(&s->demuxer)) {
            sc_daemon_session_stop(d);
            return false;
        }
        s->demuxer_started = true;

        // A report timeline starts at the first successfully retained decoded
        // frame. Do not expose READY (and therefore do not accept reportable
        // operations) until that immutable anchor exists.
        if (d->report_active) {
            if (!sc_daemon_wait_first_report_frame(d)) {
                LOGE("Test report: no decoded first frame within 15 seconds");
                sc_report_mark_failed(&d->report);
                sc_daemon_session_stop(d);
                return false;
            }
        }
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

        if (options->turn_screen_off) {
            struct sc_control_msg msg = {
                .type = SC_CONTROL_MSG_TYPE_SET_DISPLAY_POWER,
                .set_display_power.on = false,
            };
            if (!sc_controller_push_msg(&s->controller, &msg)) {
                LOGW("Could not request 'set display power'");
            }
        }
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

// Base temp directory ($TMPDIR or /tmp).
static const char *
temp_dir(void) {
    const char *t = getenv("TMPDIR");
    return (t && *t) ? t : "/tmp";
}

// Create a unique temp file "<tmp>/scrcpy-auto-<tag>-XXXXXX"; on success writes
// its malloc'd path to *out_path and returns an open fd (caller closes), else -1.
static int
make_temp_file(const char *tag, char **out_path) {
    char tmpl[512];
    snprintf(tmpl, sizeof(tmpl), "%s/scrcpy-auto-%s-XXXXXX", temp_dir(), tag);
    int fd = mkstemp(tmpl);
    if (fd < 0) {
        return -1;
    }
    *out_path = strdup(tmpl);
    if (!*out_path) {
        close(fd);
        unlink(tmpl);
        return -1;
    }
    return fd;
}

static bool
write_all_fd(int fd, const uint8_t *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, data + off, len - off);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        off += (size_t) w;
    }
    return true;
}

static bool
plugin_copy_may_continue(struct sc_daemon *d) {
    sc_mutex_lock(&d->mutex);
    bool ready = d->state == SC_DAEMON_STATE_READY && !d->stop
              && !d->session.dead;
    sc_mutex_unlock(&d->mutex);
    return ready;
}

// Copy one bounded regular file, aborting between chunks if the leased device
// session starts draining. This avoids blocking finalization on FIFOs, device
// nodes, unbounded files, or a long copy that no longer belongs to the report.
static bool
copy_plugin_file(struct sc_daemon *d, const char *src, const char *dst) {
    if (!plugin_copy_may_continue(d)) {
        return false;
    }

#ifndef _WIN32
    // Open first, then validate that exact object. O_NONBLOCK prevents a
    // stat->open FIFO substitution from stalling the session-drain path.
    int fd = open(src, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1) {
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0
            || (uint64_t) st.st_size > SC_DAEMON_MAX_FRAME_SIZE) {
        close(fd);
        return false;
    }
    FILE *in = fdopen(fd, "rb");
    if (!in) {
        close(fd);
        return false;
    }
#else
    // Windows fopen() does not block on named pipes addressed as regular
    // filesystem paths. Keep the portable regular-file validation here.
    struct stat st;
    if (stat(src, &st) || !S_ISREG(st.st_mode) || st.st_size < 0
            || (uint64_t) st.st_size > SC_DAEMON_MAX_FRAME_SIZE) {
        return false;
    }
    FILE *in = fopen(src, "rb");
    if (!in) {
        return false;
    }
#endif
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[65536];
    size_t n;
    size_t total = 0;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (n > SC_DAEMON_MAX_FRAME_SIZE - total
                || !plugin_copy_may_continue(d)
                || fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
        total += n;
    }
    if (ferror(in)) {
        ok = false;
    }
    fclose(in);
    if (fclose(out) != 0) {
        ok = false;
    }
    if (!ok) {
        unlink(dst);
    }
    return ok;
}

struct sc_plugin_assets {
    char **items;
    size_t count;
    size_t cap;
};

static void
plugin_assets_destroy(struct sc_plugin_assets *assets) {
    for (size_t i = 0; i < assets->count; ++i) {
        free(assets->items[i]);
    }
    free(assets->items);
    memset(assets, 0, sizeof(*assets));
}

static bool
plugin_assets_append(struct sc_plugin_assets *assets, const char *path) {
    if (assets->count == assets->cap) {
        size_t cap = assets->cap ? assets->cap * 2 : 8;
        if (cap > SIZE_MAX / sizeof(*assets->items)) {
            return false;
        }
        char **items = realloc(assets->items, cap * sizeof(*items));
        if (!items) {
            return false;
        }
        assets->items = items;
        assets->cap = cap;
    }
    char *copy = strdup(path);
    if (!copy) {
        return false;
    }
    assets->items[assets->count++] = copy;
    return true;
}

// Copy every declared path/pathlist input into <report>/assets/ so the report
// remains self-contained. The original values stay in inputs.named; this list
// records the copied report-relative paths.
static bool
collect_plugin_assets(struct sc_daemon *d, const struct sc_addon *addon,
                      char *const *arg_names, char *const *arg_values,
                      unsigned count, struct sc_plugin_assets *assets) {
    if (!d->opts->auto_test_report || !addon) {
        return true;
    }

    char dir[600];
    snprintf(dir, sizeof(dir), "%s/assets", d->opts->auto_test_report);
    if (mkdir(dir, 0755) && errno != EEXIST) {
        LOGW("Could not create plugin report asset directory: %s", dir);
        return false;
    }

    bool ok = true;
    for (unsigned i = 0; i < count; ++i) {
        bool is_path = false;
        for (unsigned j = 0; j < addon->arg_count; ++j) {
            if (!strcmp(addon->args[j].name, arg_names[i])
                    && addon->args[j].is_path) {
                is_path = true;
                break;
            }
        }
        if (!is_path) {
            continue;
        }

        char *dup = strdup(arg_values[i]);
        if (!dup) {
            ok = false;
            continue;
        }
        char *saveptr = NULL;
        for (char *p = strtok_r(dup, "\n", &saveptr); p;
             p = strtok_r(NULL, "\n", &saveptr)) {
            const char *bn = strrchr(p, '/');
#ifdef _WIN32
            const char *win_bn = strrchr(p, '\\');
            if (!bn || (win_bn && win_bn > bn)) {
                bn = win_bn;
            }
#endif
            bn = bn ? bn + 1 : p;
            if (!*bn) {
                ok = false;
                continue;
            }

            sc_mutex_lock(&d->mutex);
            unsigned idx = d->plugin_asset_counter++;
            sc_mutex_unlock(&d->mutex);

            char dst[1024];
            char rel[512];
            int dst_len =
                snprintf(dst, sizeof(dst), "%s/%u-%s", dir, idx, bn);
            int rel_len =
                snprintf(rel, sizeof(rel), "assets/%u-%s", idx, bn);
            if (dst_len < 0 || (size_t) dst_len >= sizeof(dst)
                    || rel_len < 0 || (size_t) rel_len >= sizeof(rel)
                    || !copy_plugin_file(d, p, dst)) {
                LOGW("Could not copy plugin input asset: %s", p);
                ok = false;
                continue;
            }
            if (!plugin_assets_append(assets, rel)) {
                unlink(dst);
                ok = false;
            }
        }
        free(dup);
    }
    return ok;
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
        && sc_json_append_escaped(buf, device_name)
        && sc_strbuf_append_staticstr(buf, ",\"video_source\":\"")
        && sc_strbuf_append_str(buf,
               d->opts->video_source == SC_VIDEO_SOURCE_CAMERA
                   ? "camera" : "display")
        && sc_strbuf_append_staticstr(buf, "\",\"flex_display\":")
        && sc_strbuf_append_str(buf,
               d->opts->flex_display ? "true" : "false");
}

// Append the type keyword for an argument (Unified Plugin Protocol §4).
static const char *
arg_type_str(const struct sc_addon_arg *arg) {
    if (arg->is_path) {
        return arg->is_list ? "pathlist" : "path";
    }
    return arg->is_list ? "list" : "string";
}

// Append ",\"plugins\":[...]" advertising each add-on's metadata. Each element
// is a keyed string "<name>[|<key>=<value>]..." (arg=/result=/meta=), a flat
// array of strings so it reads with sc_json_get_string_array. The C client
// consumes only "arg=" tokens; the web UI reads the rest (doc/addons.md §5).
static bool
append_plugins(struct sc_daemon *d, struct sc_strbuf *buf) {
    if (!sc_strbuf_append_staticstr(buf, ",\"plugins\":[")) {
        return false;
    }
    for (unsigned i = 0; i < d->addons.count; ++i) {
        const struct sc_addon *a = &d->addons.list[i];
        struct sc_strbuf spec;
        if (!sc_strbuf_init(&spec, 64)) {
            return false;
        }
        bool w = sc_strbuf_append_str(&spec, a->name);
        if (w && a->result_field) {
            w = sc_strbuf_append_staticstr(&spec, "|result=")
             && sc_strbuf_append_str(&spec, a->result_field);
        }
        for (unsigned j = 0; w && j < a->arg_count; ++j) {
            const struct sc_addon_arg *arg = &a->args[j];
            w = sc_strbuf_append_staticstr(&spec, "|arg=")
             && sc_strbuf_append_str(&spec, arg->name)
             && sc_strbuf_append_char(&spec, ':')
             && sc_strbuf_append_str(&spec, arg_type_str(arg))
             && sc_strbuf_append_char(&spec, ':')
             && sc_strbuf_append_str(&spec,
                                     arg->required ? "required" : "optional");
        }
        for (unsigned j = 0; w && j < a->meta_count; ++j) {
            w = sc_strbuf_append_staticstr(&spec, "|meta=")
             && sc_strbuf_append_str(&spec, a->metas[j].key)
             && sc_strbuf_append_char(&spec, ':')
             && sc_strbuf_append_str(&spec, a->metas[j].value);
        }
        w = w && (i == 0 || sc_strbuf_append_char(buf, ','))
          && sc_json_append_escaped(buf, spec.s);
        free(spec.s);
        if (!w) {
            return false;
        }
    }
    return sc_strbuf_append_char(buf, ']');
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
          && append_plugins(d, &buf)
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
    if (d->state != SC_DAEMON_STATE_READY || d->stop || d->session.dead) {
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

// Append ",\"report\":{...}" (the --auto-test-report location and recording
// state and timeline clocks) and ",\"config\":{...}" (the referenced capture
// parameters), so --daemon-status is a one-stop readout of how the daemon was
// launched.
static bool
append_status_extras(struct sc_daemon *d, struct sc_strbuf *buf) {
    const struct scrcpy_options *o = d->opts;

    // report: directory + codec-compatible video path + recording state
    // (append_str, not append_staticstr: the ternary is a const char*, not a
    // literal array — sizeof() would give the pointer size)
    bool w = sc_strbuf_append_staticstr(buf, ",\"report\":{\"enabled\":")
          && sc_strbuf_append_str(buf,
                 o->auto_test_report ? "true" : "false");
    if (w && o->auto_test_report) {
        int64_t recorded_ms = 0;
        int64_t source_end_ms = 0;
        sc_clip_buffer_timeline_time_ms(&d->clips, &recorded_ms);
        sc_clip_buffer_source_time_ms(&d->clips, &source_end_ms);
        int64_t held_tail_ms = recorded_ms > source_end_ms
                             ? recorded_ms - source_end_ms : 0;
        char timeline[160];
        snprintf(timeline, sizeof(timeline),
                 ",\"recorded_ms\":%" PRId64
                 ",\"source_end_ms\":%" PRId64
                 ",\"held_tail_ms\":%" PRId64,
                 recorded_ms, source_end_ms, held_tail_ms);

        w = sc_strbuf_append_staticstr(buf, ",\"dir\":")
         && sc_json_append_escaped(buf, o->auto_test_report)
         && sc_strbuf_append_staticstr(buf, ",\"recording\":")
         && sc_strbuf_append_str(buf,
                d->report_recording && !sc_report_failed(&d->report)
                    ? "true" : "false")
         && sc_strbuf_append_str(buf, timeline);
        if (w && d->report_initialized && d->report.video_path) {
            w = sc_strbuf_append_staticstr(buf, ",\"video\":")
             && sc_json_append_escaped(buf, d->report.video_path)
             && sc_strbuf_append_staticstr(buf, ",\"codec\":")
             && sc_json_append_escaped(buf, d->report.video_codec)
             && sc_strbuf_append_staticstr(buf, ",\"container\":")
             && sc_json_append_escaped(buf, d->report.video_container)
             && sc_strbuf_append_staticstr(buf, ",\"mime_type\":")
             && sc_json_append_escaped(buf, d->report.video_mime_type);
        }
    }
    w = w && sc_strbuf_append_char(buf, '}');

    // config: the capture parameters the daemon was started with
    char cfg[96];
    snprintf(cfg, sizeof(cfg),
             ",\"config\":{\"port\":%u,\"control\":%s,\"video\":%s",
             o->daemon_port, o->control ? "true" : "false",
             o->video ? "true" : "false");
    w = w && sc_strbuf_append_str(buf, cfg);
    if (w && o->video) {
        char v[128];
        snprintf(v, sizeof(v),
                 ",\"codec\":\"%s\",\"bit_rate\":%u,\"max_size\":%u",
                 sc_daemon_codec_name(o->video_codec), o->video_bit_rate,
                 o->max_size);
        w = sc_strbuf_append_str(buf, v)
         && sc_strbuf_append_staticstr(buf, ",\"max_fps\":")
         && (o->max_fps ? sc_json_append_escaped(buf, o->max_fps)
                        : sc_strbuf_append_staticstr(buf, "null"))
         && sc_strbuf_append_staticstr(buf, ",\"encoder\":")
         && (o->video_encoder ? sc_json_append_escaped(buf, o->video_encoder)
                              : sc_strbuf_append_staticstr(buf, "null"));
    }
    return w && sc_strbuf_append_char(buf, '}');
}

static void
handle_status(struct sc_daemon *d, sc_socket socket, int64_t id) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        return;
    }

    if (!append_status_fields(d, &buf)
            || !append_plugins(d, &buf)
            || !append_status_extras(d, &buf)) {
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

// Log an operation to the test report if active (defined below)
static void
report_log(struct sc_daemon *d, const char *op, const char *action,
           const char *extra);

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

    if (!ok) {
        release_session(d);
        send_error(socket, id, "E_INTERNAL", "PNG encoding failed");
        return;
    }

    // Test report: log the screenshot (timestamp + dimensions, no image copy)
    if (d->report_active) {
        char *action = NULL;
        sc_json_get_string(json, "action", &action);
        char rextra[64];
        snprintf(rextra, sizeof(rextra),
                 "\"video_size\":{\"w\":%d,\"h\":%d}", width, height);
        report_log(d, "screencap", action, rextra);
        free(action);
    }
    release_session(d);

    char extra[128];
    snprintf(extra, sizeof(extra),
             "\"width\":%d,\"height\":%d,\"frame_age_ms\":%" PRId64
             ",\"payload_len\":%zu",
             width, height, (int64_t) SC_TICK_TO_MS(age), png_size);
    send_ok(socket, id, extra, png, png_size);
    free(png);
}

// Extract [start_ms, end_ms] of the current recording into a
// codec-compatible container (MP4, or WebM for VP8). The clip bytes are
// returned as the response payload; the CLIENT writes the output file, so this
// also works remotely and needs no daemon-side write access. The start snaps
// back to the nearest keyframe. Availability is checked against the
// report/session wall clock, not the last packet PTS: Android may emit no
// encoded packet while a frame stays static.
static void
handle_clip(struct sc_daemon *d, sc_socket socket, int64_t id,
            struct sc_json *json) {
    if (!d->opts->video) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "video is disabled (--no-video)");
        return;
    }
    if (d->opts->flex_display) {
        send_error(socket, id, "E_UNSUPPORTED",
                   "clip extraction is unavailable for a dynamically resized "
                   "flex display");
        return;
    }
    int64_t start_ms, end_ms;
    if (!sc_json_get_int64(json, "start_ms", &start_ms)
            || !sc_json_get_int64(json, "end_ms", &end_ms)
            || start_ms < 0 || end_ms <= start_ms) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "expected start_ms/end_ms with 0 <= start < end");
        return;
    }

    if (!acquire_session(d, socket, id)) {
        return;
    }

    int64_t recorded_ms;
    if (!sc_clip_buffer_timeline_time_ms(&d->clips, &recorded_ms)) {
        release_session(d);
        send_error(socket, id, "E_RANGE", "no video has been recorded yet");
        return;
    }

    uint8_t *payload;
    size_t payload_size;
    const struct sc_clip_format *format;
    int64_t actual_start_ms, actual_end_ms, source_end_ms, held_tail_ms;
    char err[192];
    int r = sc_clip_buffer_extract(&d->clips, start_ms, end_ms, recorded_ms,
                                   &payload, &payload_size, &format,
                                   &actual_start_ms, &actual_end_ms,
                                   &source_end_ms, &held_tail_ms, err,
                                   sizeof(err));
    release_session(d);
    if (r) {
        const char *code = r == SC_CLIP_ERANGE ? "E_RANGE"
                         : r == SC_CLIP_ESESSION ? "E_SESSION"
                         : r == SC_CLIP_ETOOLARGE ? "E_TOO_LARGE"
                         : "E_INTERNAL";
        send_error(socket, id, code, err);
        return;
    }

    if (payload_size > SC_DAEMON_MAX_BINARY_PAYLOAD) {
        av_free(payload);
        send_error(socket, id, "E_TOO_LARGE",
                   "clip exceeds the 1 GiB daemon payload limit; split the "
                   "requested range");
        return;
    }

    const char *codec = sc_daemon_codec_name(d->opts->video_codec);
    char extra[512];
    snprintf(extra, sizeof(extra),
             "\"start_ms\":%" PRId64 ",\"end_ms\":%" PRId64
             ",\"source_end_ms\":%" PRId64
             ",\"held_tail_ms\":%" PRId64
             ",\"codec\":\"%s\",\"container\":\"%s\""
             ",\"extension\":\"%s\",\"mime_type\":\"%s\""
             ",\"payload_len\":%zu",
             actual_start_ms, actual_end_ms, source_end_ms, held_tail_ms,
             codec, format->container, format->extension, format->mime_type,
             payload_size);
    send_ok(socket, id, extra, payload, payload_size);
    av_free(payload);
}

static void
handle_control(struct sc_daemon *d, sc_socket socket, int64_t id,
               const struct sc_json *json, unsigned conn_index) {
    if (!d->opts->control) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "control is disabled (--no-control)");
        return;
    }
    if (d->opts->video_source == SC_VIDEO_SOURCE_CAMERA) {
        send_error(socket, id, "E_UNSUPPORTED",
                   "touch/text control is unavailable for camera capture; "
                   "use camera_set_torch or camera_zoom_in/camera_zoom_out");
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
    // disjoint pointer ids (doc/daemon.md §9.6): at most
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

    // Test report: log the gesture at its start (t_ms = when it began); the
    // raw cmds + video size let the web renderer reconstruct the finger paths
    if (d->report_active) {
        char *action = NULL;
        sc_json_get_string(json, "action", &action);
        struct sc_strbuf eb;
        if (sc_strbuf_init(&eb, 128)) {
            char sz[64];
            snprintf(sz, sizeof(sz),
                     "\"video_size\":{\"w\":%u,\"h\":%u},\"cmds\":[",
                     screen_size.width, screen_size.height);
            bool w = sc_strbuf_append_str(&eb, sz);
            for (unsigned i = 0; w && i < count; ++i) {
                if (i) {
                    w = sc_strbuf_append_char(&eb, ',');
                }
                w = w && sc_json_append_escaped(&eb, cmds[i]);
            }
            w = w && sc_strbuf_append_char(&eb, ']');
            if (w) {
                report_log(d, "control", action, eb.s);
            }
            free(eb.s);
        }
        free(action);
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

static void
handle_device_command(struct sc_daemon *d, sc_socket socket, int64_t id,
                      enum sc_control_msg_type type, const char *op,
                      bool display_only) {
    if (!d->opts->control) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "control is disabled (--no-control)");
        return;
    }
    if (display_only
            && d->opts->video_source == SC_VIDEO_SOURCE_CAMERA) {
        send_error(socket, id, "E_UNSUPPORTED",
                   "this command is unavailable for camera capture");
        return;
    }
    if (!acquire_session(d, socket, id)) {
        return;
    }

    struct sc_control_msg msg = {
        .type = type,
    };
    bool ok = sc_controller_push_msg(&d->session.controller, &msg);
    if (ok) {
        report_log(d, op, NULL, NULL);
    }
    release_session(d);

    if (ok) {
        send_ok(socket, id, NULL, NULL, 0);
    } else {
        send_error(socket, id, "E_INTERNAL",
                   "could not enqueue the device command");
    }
}

static void
handle_set_display_power(struct sc_daemon *d, sc_socket socket, int64_t id,
                         const struct sc_json *json) {
    bool on;
    if (!sc_json_get_bool(json, "on", &on)) {
        send_error(socket, id, "E_BAD_REQUEST", "expected boolean \"on\"");
        return;
    }
    if (!d->opts->control) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "control is disabled (--no-control)");
        return;
    }
    if (!acquire_session(d, socket, id)) {
        return;
    }

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_SET_DISPLAY_POWER,
        .set_display_power.on = on,
    };
    bool ok = sc_controller_push_msg(&d->session.controller, &msg);
    if (ok) {
        report_log(d, "set_display_power", on ? "on" : "off", NULL);
    }
    release_session(d);

    if (ok) {
        send_ok(socket, id, NULL, NULL, 0);
    } else {
        send_error(socket, id, "E_INTERNAL", "could not enqueue event");
    }
}

// ---- realtime input injection (single, non-blocking events) ----

static int64_t
json_int_or(const struct sc_json *json, const char *key, int64_t def) {
    int64_t v;
    return sc_json_get_int64(json, key, &v) ? v : def;
}

// Log an operation to the test report if active. `action` and `extra` may be
// NULL. Takes ownership of nothing.
static void
report_log(struct sc_daemon *d, const char *op, const char *action,
           const char *extra) {
    if (d->report_active && d->report_initialized) {
        sc_report_log_event(&d->report, op, action, extra);
    }
}

// Parse an "action" string into a motion action; returns false if invalid
static bool
parse_motion_action(const struct sc_json *json,
                    enum android_motionevent_action *out) {
    char *s;
    if (!sc_json_get_string(json, "action", &s)) {
        return false;
    }
    bool ok = true;
    if (!strcmp(s, "down")) {
        *out = AMOTION_EVENT_ACTION_DOWN;
    } else if (!strcmp(s, "up")) {
        *out = AMOTION_EVENT_ACTION_UP;
    } else if (!strcmp(s, "move")) {
        *out = AMOTION_EVENT_ACTION_MOVE;
    } else if (!strcmp(s, "hover_move")) {
        *out = AMOTION_EVENT_ACTION_HOVER_MOVE;
    } else {
        ok = false;
    }
    free(s);
    return ok;
}

// Ready + control gate common to the inject_* ops; on success in_flight was
// incremented (release_session() must be called)
static bool
inject_acquire(struct sc_daemon *d, sc_socket socket, int64_t id) {
    if (!d->opts->control) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "control is disabled (--no-control)");
        return false;
    }
    if (d->opts->video_source == SC_VIDEO_SOURCE_CAMERA) {
        send_error(socket, id, "E_UNSUPPORTED",
                   "key/touch/text/scroll injection is unavailable for "
                   "camera capture");
        return false;
    }
    return acquire_session(d, socket, id);
}

static bool
camera_acquire(struct sc_daemon *d, sc_socket socket, int64_t id) {
    if (!d->opts->control) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "control is disabled (--no-control)");
        return false;
    }
    if (d->opts->video_source != SC_VIDEO_SOURCE_CAMERA) {
        send_error(socket, id, "E_UNSUPPORTED",
                   "camera control requires --video-source=camera");
        return false;
    }
    return acquire_session(d, socket, id);
}

static void
reply_push(struct sc_daemon *d, sc_socket socket, int64_t id, bool pushed);

static void
handle_camera_set_torch(struct sc_daemon *d, sc_socket socket, int64_t id,
                        const struct sc_json *json) {
    bool on;
    if (!sc_json_get_bool(json, "on", &on)) {
        send_error(socket, id, "E_BAD_REQUEST", "expected boolean \"on\"");
        return;
    }
    if (!camera_acquire(d, socket, id)) {
        return;
    }
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_CAMERA_SET_TORCH,
        .camera_set_torch = {
            .on = on,
        },
    };
    reply_push(d, socket, id,
               sc_controller_push_msg(&d->session.controller, &msg));
}

static void
handle_camera_zoom(struct sc_daemon *d, sc_socket socket, int64_t id,
                   bool zoom_in) {
    if (!camera_acquire(d, socket, id)) {
        return;
    }
    struct sc_control_msg msg = {
        .type = zoom_in ? SC_CONTROL_MSG_TYPE_CAMERA_ZOOM_IN
                        : SC_CONTROL_MSG_TYPE_CAMERA_ZOOM_OUT,
    };
    reply_push(d, socket, id,
               sc_controller_push_msg(&d->session.controller, &msg));
}

static void
handle_resize_display(struct sc_daemon *d, sc_socket socket, int64_t id,
                      const struct sc_json *json) {
    int64_t width;
    int64_t height;
    if (!sc_json_get_int64(json, "width", &width)
            || !sc_json_get_int64(json, "height", &height)
            || width <= 0 || width > UINT16_MAX
            || height <= 0 || height > UINT16_MAX) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "expected width/height in 1..65535");
        return;
    }
    if (!d->opts->flex_display) {
        send_error(socket, id, "E_UNSUPPORTED",
                   "resize_display requires --flex-display");
        return;
    }
    if (!acquire_session(d, socket, id)) {
        return;
    }
    sc_controller_resize_display(&d->session.controller, (uint16_t) width,
                                 (uint16_t) height);
    release_session(d);
    send_ok(socket, id, NULL, NULL, 0);
}

static void
reply_push(struct sc_daemon *d, sc_socket socket, int64_t id, bool pushed) {
    release_session(d);
    if (pushed) {
        send_ok(socket, id, NULL, NULL, 0);
    } else {
        send_error(socket, id, "E_INTERNAL", "could not enqueue event");
    }
}

static void
handle_inject_touch(struct sc_daemon *d, sc_socket socket, int64_t id,
                    const struct sc_json *json, unsigned conn_index) {
    enum android_motionevent_action action;
    int64_t x, y;
    if (!parse_motion_action(json, &action)
            || !sc_json_get_int64(json, "x", &x)
            || !sc_json_get_int64(json, "y", &y)) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "expected action(down|up|move|hover_move), x, y");
        return;
    }
    int64_t pressure_u16 = json_int_or(
        json, "pressure_u16",
        action == AMOTION_EVENT_ACTION_UP ? 0 : UINT16_MAX);
    int64_t action_button = json_int_or(json, "action_button", 0);
    int64_t buttons = json_int_or(json, "buttons", 0);
    if (pressure_u16 < 0 || pressure_u16 > UINT16_MAX
            || action_button < 0 || action_button > UINT32_MAX
            || buttons < 0 || buttons > UINT32_MAX) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "invalid pressure_u16/action_button/buttons");
        return;
    }

    if (!inject_acquire(d, socket, id)) {
        return;
    }

    struct sc_size size = {UINT16_MAX, UINT16_MAX};
    if (d->opts->video
            && !sc_frame_keeper_wait_size(&d->keeper,
                                          sc_tick_now()
                                              + SC_DAEMON_SCREENCAP_DEADLINE,
                                          &size)) {
        release_session(d);
        send_error(socket, id, "E_NOT_READY", "no video frame received yet");
        return;
    }

    int64_t report_pointer_id = json_int_or(json, "pointer_id", 0);
    uint64_t pointer_id = (uint64_t) report_pointer_id;
    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    msg.inject_touch_event.action = action;
    msg.inject_touch_event.action_button =
        (enum android_motionevent_buttons) action_button;
    msg.inject_touch_event.buttons =
        (enum android_motionevent_buttons) buttons;
    msg.inject_touch_event.pointer_id = pointer_id;
    msg.inject_touch_event.position.screen_size = size;
    msg.inject_touch_event.position.point.x = (int32_t) x;
    msg.inject_touch_event.position.point.y = (int32_t) y;
    msg.inject_touch_event.pressure = pressure_u16 == UINT16_MAX
                                    ? 1.0f
                                    : (float) pressure_u16 / 65536.0f;

    bool pushed = sc_controller_push_msg(&d->session.controller, &msg);
    if (pushed && d->report_active) {
        struct sc_touch_report_summary summary;
        if (sc_touch_report_feed(&d->conns[conn_index].touch_report, action,
                                 pointer_id, (int32_t) x, (int32_t) y,
                                 sc_tick_now(), &summary)) {
            char extra[320];
            snprintf(extra, sizeof(extra),
                     "\"video_size\":{\"w\":%u,\"h\":%u},"
                     "\"x\":%" PRId32 ",\"y\":%" PRId32 ","
                     "\"pointer_id\":%" PRId64 ","
                     "\"start_x\":%" PRId32 ",\"start_y\":%" PRId32 ","
                     "\"duration_ms\":%" PRId64 ","
                     "\"sample_count\":%" PRIu32,
                     size.width, size.height, summary.x, summary.y,
                     report_pointer_id,
                     summary.start_x, summary.start_y, summary.duration_ms,
                     summary.sample_count);
            report_log(d, "inject_touch", "up", extra);
        }
    }
    reply_push(d, socket, id, pushed);
}

static void
handle_inject_key(struct sc_daemon *d, sc_socket socket, int64_t id,
                  const struct sc_json *json) {
    char *action_str;
    int64_t keycode;
    if (!sc_json_get_string(json, "action", &action_str)
            || !sc_json_get_int64(json, "keycode", &keycode)) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "expected action(down|up), keycode");
        return;
    }
    enum android_keyevent_action action;
    bool valid = true;
    if (!strcmp(action_str, "down")) {
        action = AKEY_EVENT_ACTION_DOWN;
    } else if (!strcmp(action_str, "up")) {
        action = AKEY_EVENT_ACTION_UP;
    } else {
        valid = false;
    }
    free(action_str);
    if (!valid) {
        send_error(socket, id, "E_BAD_REQUEST", "action must be down or up");
        return;
    }

    if (!inject_acquire(d, socket, id)) {
        return;
    }

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE;
    msg.inject_keycode.action = action;
    msg.inject_keycode.keycode = (enum android_keycode) keycode;
    msg.inject_keycode.repeat = (uint32_t) json_int_or(json, "repeat", 0);
    msg.inject_keycode.metastate =
        (enum android_metastate) json_int_or(json, "metastate", 0);

    reply_push(d, socket, id,
               sc_controller_push_msg(&d->session.controller, &msg));
}

static void
handle_inject_text(struct sc_daemon *d, sc_socket socket, int64_t id,
                   const struct sc_json *json) {
    char *text;
    if (!sc_json_get_string(json, "text", &text)) {
        send_error(socket, id, "E_BAD_REQUEST", "expected text");
        return;
    }

    if (!inject_acquire(d, socket, id)) {
        free(text);
        return;
    }

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = text; // ownership moves to the controller on success

    bool pushed = sc_controller_push_msg(&d->session.controller, &msg);
    if (!pushed) {
        free(text);
    }
    reply_push(d, socket, id, pushed);
}

static void
handle_inject_scroll(struct sc_daemon *d, sc_socket socket, int64_t id,
                     const struct sc_json *json) {
    int64_t x, y;
    if (!sc_json_get_int64(json, "x", &x)
            || !sc_json_get_int64(json, "y", &y)) {
        send_error(socket, id, "E_BAD_REQUEST", "expected x, y, hscroll, "
                   "vscroll");
        return;
    }

    if (!inject_acquire(d, socket, id)) {
        return;
    }

    struct sc_size size = {UINT16_MAX, UINT16_MAX};
    if (d->opts->video
            && !sc_frame_keeper_wait_size(&d->keeper,
                                          sc_tick_now()
                                              + SC_DAEMON_SCREENCAP_DEADLINE,
                                          &size)) {
        release_session(d);
        send_error(socket, id, "E_NOT_READY", "no video frame received yet");
        return;
    }

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT;
    msg.inject_scroll_event.position.screen_size = size;
    msg.inject_scroll_event.position.point.x = (int32_t) x;
    msg.inject_scroll_event.position.point.y = (int32_t) y;
    msg.inject_scroll_event.hscroll = (float) json_int_or(json, "hscroll", 0);
    msg.inject_scroll_event.vscroll = (float) json_int_or(json, "vscroll", 0);
    msg.inject_scroll_event.buttons =
        (enum android_motionevent_buttons) json_int_or(json, "buttons", 0);

    reply_push(d, socket, id,
               sc_controller_push_msg(&d->session.controller, &msg));
}

// Standalone test-report annotation ("title: description"), not tied to any
// control command. Logged as a note event; always acked.
static void
handle_note(struct sc_daemon *d, sc_socket socket, int64_t id,
            const struct sc_json *json) {
    char *note;
    if (!sc_json_get_string(json, "note", &note)) {
        send_error(socket, id, "E_BAD_REQUEST", "expected \"note\"");
        return;
    }

    bool acquired = false;
    if (d->report_active) {
        if (!acquire_session(d, socket, id)) {
            free(note);
            return;
        }
        acquired = true;

        struct sc_strbuf eb;
        if (sc_strbuf_init(&eb, 128)) {
            const char *colon = strchr(note, ':');
            bool w;
            if (colon) {
                size_t tlen = colon - note;
                while (tlen > 0 && note[tlen - 1] == ' ') {
                    tlen--; // trim trailing spaces of the title
                }
                char *title = strndup(note, tlen);
                const char *text = colon + 1;
                while (*text == ' ') {
                    text++; // trim leading spaces of the description
                }
                w = sc_strbuf_append_staticstr(&eb, "\"title\":")
                 && sc_json_append_escaped(&eb, title ? title : "")
                 && sc_strbuf_append_staticstr(&eb, ",\"text\":")
                 && sc_json_append_escaped(&eb, text);
                free(title);
            } else {
                w = sc_strbuf_append_staticstr(&eb, "\"title\":\"note\",\"text\":")
                 && sc_json_append_escaped(&eb, note);
            }
            if (w) {
                report_log(d, "note", NULL, eb.s);
            }
            free(eb.s);
        }
    }

    free(note);
    if (acquired) {
        release_session(d);
    }
    send_ok(socket, id, NULL, NULL, 0);
}

// Build a malloc'd "SC_ARG_<NAME>=<value>" env entry, upper-casing NAME and
// replacing any non-alphanumeric character with '_' (so `env` always gets a
// valid identifier). Returns NULL on OOM.
static char *
make_arg_env(const char *arg_name, const char *value) {
    struct sc_strbuf b;
    if (!sc_strbuf_init(&b, 32)) {
        return NULL;
    }
    bool w = sc_strbuf_append_staticstr(&b, "SC_ARG_");
    for (const char *p = arg_name; w && *p; ++p) {
        char c = *p;
        if (c >= 'a' && c <= 'z') {
            c = (char) (c - 'a' + 'A');
        } else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            c = '_';
        }
        w = sc_strbuf_append_char(&b, c);
    }
    w = w && sc_strbuf_append_char(&b, '=')
          && sc_strbuf_append_str(&b, value ? value : "");
    if (!w) {
        free(b.s);
        return NULL;
    }
    return b.s;
}

// ---- long-running "service" add-ons (doc/addons.md) ----

static void
service_sleep_ms(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts = { ms / 1000, (long) (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
#endif
}

enum sc_service_process_state {
    SC_SERVICE_PROCESS_RUNNING,
    SC_SERVICE_PROCESS_EXITED,
    SC_SERVICE_PROCESS_ERROR,
};

// Observe without closing/reaping. The connection keeps exclusive ownership
// of the pid/handle until it clears plugin_pid under the daemon mutex, which
// prevents a drain thread from terminating a stale, already-reused identity.
static enum sc_service_process_state
service_observe(sc_pid pid, sc_exit_code *out_code) {
#ifdef _WIN32
    DWORD code;
    if (!GetExitCodeProcess(pid, &code)) {
        return SC_SERVICE_PROCESS_ERROR;
    }
    if (code == STILL_ACTIVE) {
        return SC_SERVICE_PROCESS_RUNNING;
    }
    *out_code = (sc_exit_code) code;
    return SC_SERVICE_PROCESS_EXITED;
#else
    siginfo_t info;
    info.si_pid = 0;
    if (waitid(P_PID, pid, &info, WEXITED | WNOHANG | WNOWAIT) == -1) {
        if (errno == EINTR) {
            return SC_SERVICE_PROCESS_RUNNING;
        }
        return SC_SERVICE_PROCESS_ERROR;
    }
    if (info.si_pid == 0) {
        return SC_SERVICE_PROCESS_RUNNING;
    }
    *out_code = (info.si_code == CLD_EXITED) ? info.si_status
                                             : SC_EXIT_CODE_NONE;
    return SC_SERVICE_PROCESS_EXITED;
#endif
}

static enum sc_service_process_state
wait_process_until(sc_pid pid, sc_tick deadline, sc_exit_code *out_code) {
    for (;;) {
        enum sc_service_process_state state = service_observe(pid, out_code);
        if (state != SC_SERVICE_PROCESS_RUNNING) {
            return state;
        }
        if (deadline && sc_tick_now() >= deadline) {
            return SC_SERVICE_PROCESS_RUNNING;
        }
        service_sleep_ms(SC_SERVICE_POLL_MS);
    }
}

// Wait indefinitely while the leased device session is usable. Once draining
// begins, force termination and bound the remaining wait so a failed kill (or
// an uninterruptible child) cannot prevent completion-event persistence.
static enum sc_service_process_state
plugin_wait_for_exit(struct sc_daemon *d, unsigned conn_index, sc_pid pid,
                     sc_exit_code *out_code) {
    sc_tick kill_deadline = 0;
    for (;;) {
        enum sc_service_process_state state = service_observe(pid, out_code);
        if (state != SC_SERVICE_PROCESS_RUNNING) {
            return state;
        }

        sc_mutex_lock(&d->mutex);
        bool draining = d->state != SC_DAEMON_STATE_READY || d->stop
                     || d->session.dead;
        bool owned = d->conns[conn_index].plugin_pid == pid;
        sc_mutex_unlock(&d->mutex);
        if (!owned) {
            return SC_SERVICE_PROCESS_ERROR;
        }

        if (draining && !kill_deadline) {
            if (!sc_process_terminate(pid)) {
                // A racing exit is success; only a still-running/error state
                // loses ownership. Crucially, never enter an infinite wait
                // after the termination request itself failed.
                state = service_observe(pid, out_code);
                return state == SC_SERVICE_PROCESS_RUNNING
                     ? SC_SERVICE_PROCESS_ERROR : state;
            }
            kill_deadline =
                sc_tick_now() + SC_TICK_FROM_MS(SC_SERVICE_KILL_WAIT_MS);
        }
        if (kill_deadline && sc_tick_now() >= kill_deadline) {
            return SC_SERVICE_PROCESS_ERROR;
        }
        service_sleep_ms(SC_SERVICE_POLL_MS);
    }
}

// Force a process to stop, but never wait forever. The returned EXITED state
// still owns an unreaped process/handle; the caller must sc_process_close().
static enum sc_service_process_state
terminate_process_bounded(sc_pid pid, sc_exit_code *out_code) {
    if (!sc_process_terminate(pid)) {
        enum sc_service_process_state state = service_observe(pid, out_code);
        return state == SC_SERVICE_PROCESS_RUNNING
             ? SC_SERVICE_PROCESS_ERROR : state;
    }
    enum sc_service_process_state state =
        wait_process_until(
            pid,
            sc_tick_now() + SC_TICK_FROM_MS(SC_SERVICE_KILL_WAIT_MS),
            out_code);
    return state == SC_SERVICE_PROCESS_RUNNING
         ? SC_SERVICE_PROCESS_ERROR : state;
}

// Terminate an adopted service: SIGTERM, a short grace period, then SIGKILL.
// Reap on success; on a lost process identity, return without blocking.
static bool
terminate_service(sc_pid pid) {
#ifndef _WIN32
    kill(pid, SIGTERM);
    for (unsigned waited = 0; waited < SC_SERVICE_TERM_GRACE_MS;
         waited += SC_SERVICE_POLL_MS) {
        sc_exit_code code;
        enum sc_service_process_state state = service_observe(pid, &code);
        if (state == SC_SERVICE_PROCESS_EXITED) {
            sc_process_close(pid);
            return true;
        }
        if (state == SC_SERVICE_PROCESS_ERROR) {
            break;
        }
        service_sleep_ms(SC_SERVICE_POLL_MS);
    }
#endif
    sc_exit_code code = SC_EXIT_CODE_NONE;
    enum sc_service_process_state state =
        terminate_process_bounded(pid, &code);
    if (state == SC_SERVICE_PROCESS_EXITED) {
        sc_process_close(pid);
        return true;
    }
    LOGE("Could not terminate add-on process within %u ms",
         SC_SERVICE_KILL_WAIT_MS);
#ifdef _WIN32
    // Closing a HANDLE does not wait for or terminate the underlying process.
    // Do not leak it after ownership could no longer be observed.
    sc_process_close(pid);
#endif
    return false;
}

struct sc_service_plugin_outcome {
    bool ready;
    bool timed_out;
    bool exited;
    bool adopted;
    bool adopt_failed;
    bool process_error;
    bool session_ended;
    bool has_exit_code;
    sc_exit_code exit_code;
    enum sc_plugin_result_status result_status;
    struct sc_plugin_result result;
};

// Wait until a service writes one complete valid result object (its readiness
// signal), exits, or reaches the deadline. A partial JSON file is not ready.
// A still-running process is adopted even on timeout, preserving the existing
// daemon-managed lifetime while making that degraded outcome explicit.
static struct sc_service_plugin_outcome
wait_and_maybe_adopt_service(struct sc_daemon *d, const struct sc_addon *addon,
                             sc_pid pid, const char *result_path,
                             unsigned conn_index) {
    struct sc_service_plugin_outcome outcome = {
        .result_status = SC_PLUGIN_RESULT_MISSING,
    };

    sc_tick deadline =
        sc_tick_now() + SC_TICK_FROM_MS(SC_SERVICE_READY_TIMEOUT_MS);
    while (sc_tick_now() < deadline) {
        struct sc_plugin_result candidate;
        enum sc_plugin_result_status result_status =
            sc_plugin_result_read(result_path, SC_PLUGIN_RESULT_MAX_SIZE,
                                  &candidate);
        outcome.result_status = result_status;
        if (result_status == SC_PLUGIN_RESULT_VALID) {
            outcome.result = candidate;
            outcome.ready = true;
            break;
        }
        sc_plugin_result_destroy(&candidate);

        enum sc_service_process_state process_state =
            service_observe(pid, &outcome.exit_code);
        if (process_state == SC_SERVICE_PROCESS_EXITED) {
            outcome.exited = true;
            outcome.has_exit_code =
                outcome.exit_code != SC_EXIT_CODE_NONE;
            break;
        }
        if (process_state == SC_SERVICE_PROCESS_ERROR) {
            outcome.process_error = true;
            break;
        }

        sc_mutex_lock(&d->mutex);
        bool session_ready =
            d->state == SC_DAEMON_STATE_READY && !d->stop
            && !d->session.dead;
        sc_mutex_unlock(&d->mutex);
        if (!session_ready) {
            outcome.session_ended = true;
            break;
        }
        service_sleep_ms(SC_SERVICE_POLL_MS);
    }

    if (!outcome.exited && !outcome.process_error) {
        // It reported ready (or timed out); close the race with a process exit.
        enum sc_service_process_state process_state =
            service_observe(pid, &outcome.exit_code);
        outcome.exited = process_state == SC_SERVICE_PROCESS_EXITED;
        outcome.process_error = process_state == SC_SERVICE_PROCESS_ERROR;
        outcome.has_exit_code = outcome.exited
                             && outcome.exit_code != SC_EXIT_CODE_NONE;
    }
    outcome.timed_out = !outcome.ready && !outcome.exited
                     && !outcome.process_error && !outcome.session_ended;

    // A one-shot service may write its final result and exit between polls.
    if (outcome.exited && !outcome.ready) {
        struct sc_plugin_result final_result;
        enum sc_plugin_result_status final_status =
            sc_plugin_result_read(result_path, SC_PLUGIN_RESULT_MAX_SIZE,
                                  &final_result);
        outcome.result_status = final_status;
        if (final_status == SC_PLUGIN_RESULT_VALID) {
            outcome.result = final_result;
        } else {
            sc_plugin_result_destroy(&final_result);
        }
    }

    sc_mutex_lock(&d->mutex);
    d->conns[conn_index].plugin_pid = SC_PROCESS_NONE;
    bool session_ready =
        d->state == SC_DAEMON_STATE_READY && !d->stop && !d->session.dead;
    outcome.session_ended =
        outcome.session_ended
        || (!outcome.exited && !outcome.process_error && !session_ready);
    if (!outcome.exited && !outcome.process_error && session_ready) {
        if (d->service_count < SC_MAX_SERVICES) {
            char *name = strdup(addon->name);
            if (name) {
                d->services[d->service_count].name = name;
                d->services[d->service_count].pid = pid;
                d->service_count++;
                outcome.adopted = true;
            }
        } else {
            LOGW("Too many service add-ons (max %d); terminating \"%s\"",
                 SC_MAX_SERVICES, addon->name);
        }
    }
    sc_mutex_unlock(&d->mutex);

    if (outcome.exited) {
        // plugin_pid no longer exposes this identity; it is now safe to reap.
        sc_process_close(pid);
    } else if (outcome.process_error) {
        // Ownership could no longer be proven (for example ECHILD or an
        // invalid HANDLE). Never signal a possibly reused process identity.
        LOGE("Lost ownership of service add-on process \"%s\"", addon->name);
#ifdef _WIN32
        // CloseHandle only releases this process' reference; it does not kill
        // a still-running child.
        sc_process_close(pid);
#endif
    } else if (!outcome.adopted) {
        outcome.adopt_failed = !outcome.process_error && session_ready;
        terminate_service(pid); // overflow / OOM: don't leak the process
    }
    if (outcome.adopted) {
        LOGI("Service add-on \"%s\" started and adopted (pid %ld)",
             addon->name, (long) pid);
    }
    return outcome;
}

// Run a loaded add-on (doc/addons.md). The op runs the entrypoint script with
// the command value as $1 and runtime info in the environment (including every
// declared argument as SC_ARG_<NAME>). A normal add-on blocks until it exits;
// a `service=true` add-on that stays alive after reporting ready is adopted and
// terminated at daemon shutdown. The script's own screenshot/note/click steps
// come back as separate requests on other connections, logged in real time.
static void
handle_plugin(struct sc_daemon *d, sc_socket socket, int64_t id,
              const struct sc_json *json, unsigned conn_index) {
    char *name = NULL;
    if (!sc_json_get_string(json, "name", &name)) {
        send_error(socket, id, "E_BAD_REQUEST", "expected plugin \"name\"");
        return;
    }
    char *args = NULL;
    sc_json_get_string(json, "args", &args); // optional
    struct sc_plugin_assets assets = {0};
    struct sc_plugin_result result = {0};
    enum sc_plugin_result_status result_status = SC_PLUGIN_RESULT_MISSING;
    const char *result_json = NULL;
    char *result_path = NULL;

    // Optional named arguments (parallel string arrays), each exported to the
    // script as SC_ARG_<NAME>; the primary value stays available as $1.
    char *arg_names[SC_MAX_ADDON_ARGS] = {0};
    char *arg_values[SC_MAX_ADDON_ARGS] = {0};
    unsigned name_count = 0, value_count = 0;
    bool has_names = sc_json_get_string_array(json, "arg_names", arg_names,
                                              SC_MAX_ADDON_ARGS, &name_count);
    bool has_values = sc_json_get_string_array(json, "arg_values", arg_values,
                                               SC_MAX_ADDON_ARGS, &value_count);
    if (has_names != has_values || name_count != value_count) {
        send_error(socket, id, "E_BAD_REQUEST",
                   "arg_names/arg_values must be matching string arrays");
        goto cleanup;
    }

    const struct sc_addon *addon = sc_addons_get(&d->addons, name);
    if (!addon) {
        char msg[128];
        snprintf(msg, sizeof(msg), "no add-on provides --%s", name);
        send_error(socket, id, "E_BAD_REQUEST", msg);
        goto cleanup;
    }

    // A plugin invocation owns a session lease through completion-event
    // persistence. This keeps the report gate open if the device disconnects
    // while the child is running.
    if (!acquire_session(d, socket, id)) {
        goto cleanup;
    }
    bool acquired = true;
    bool send_response = true;
    bool response_ok = false;
    const char *response_error_code = "E_INTERNAL";
    char response_error[256] = "plugin invocation failed";

    bool report_timed =
        d->report_active && atomic_load(&d->report_initialized);
    int64_t start_ms = 0;
    int64_t end_ms = 0;
    if (report_timed
            && !sc_report_get_timeline_time_ms(&d->report, &start_ms)) {
        sc_report_mark_failed(&d->report);
        snprintf(response_error, sizeof(response_error),
                 "report timeline anchor is unavailable");
        goto finish;
    }

    const char *status = "start_failed";
    bool has_exit_code = false;
    int64_t exit_code = 0;
    bool adopted = false;

    if (!collect_plugin_assets(d, addon, arg_names, arg_values, name_count,
                               &assets)) {
        // The original path inputs remain in the event, but a report missing
        // one of its promised bundled assets must not be marked finalized.
        if (report_timed) {
            sc_report_mark_failed(&d->report);
        }
    }

    const char *missing = sc_addon_missing_env(addon);
    if (missing) {
        status = "missing_env";
        response_error_code = "E_PLUGIN";
        snprintf(response_error, sizeof(response_error),
                 "add-on --%s requires environment variable %s", name,
                 missing);
        goto finish_with_assets;
    }

    // Runtime info for the script
    char serial[256];
    char real_device_udid[256];
    sc_mutex_lock(&d->mutex);
    snprintf(serial, sizeof(serial), "%s", d->serial ? d->serial : "");
    snprintf(real_device_udid, sizeof(real_device_udid), "%s",
             d->real_device_udid_initialized && d->real_device_udid[0]
                 ? d->real_device_udid : serial);
    sc_mutex_unlock(&d->mutex);

    struct sc_size size = {0, 0};
    if (d->opts->video) {
        sc_frame_keeper_wait_size(&d->keeper,
                                  sc_tick_now() + SC_TICK_FROM_MS(200), &size);
    }
    char *exe = sc_file_get_executable_path();

    const struct scrcpy_options *o = d->opts;
    char e_port[32], e_host[40], e_serial[300], e_real_udid[300], e_report[600];
    char e_name[160], e_w[32], e_h[32], e_exe[1200];
    char e_codec[24], e_brate[32], e_mfps[40], e_msize[32], e_enc[300],
         e_ctrl[16];
    snprintf(e_port, sizeof(e_port), "SC_DAEMON_PORT=%u", o->daemon_port);
    snprintf(e_host, sizeof(e_host), "SC_DAEMON_HOST=127.0.0.1");
    snprintf(e_serial, sizeof(e_serial), "SC_DEVICE_SERIAL=%s", serial);
    snprintf(e_real_udid, sizeof(e_real_udid), "SC_REAL_DEVICE_UDID=%s",
             real_device_udid);
    snprintf(e_report, sizeof(e_report), "SC_REPORT_DIR=%s",
             o->auto_test_report ? o->auto_test_report : "");
    snprintf(e_name, sizeof(e_name), "SC_ADDON_NAME=%s", name);
    snprintf(e_w, sizeof(e_w), "SC_VIDEO_WIDTH=%u", size.width);
    snprintf(e_h, sizeof(e_h), "SC_VIDEO_HEIGHT=%u", size.height);
    snprintf(e_exe, sizeof(e_exe), "SCRCPY_AUTO=%s", exe ? exe : "scrcpy-auto");
    // Capture parameters (the daemon's launch config), so plugins read them
    // from the environment instead of shelling back into the client. Empty
    // string / 0 mirrors "unset / server default", as with SC_REPORT_DIR.
    snprintf(e_codec, sizeof(e_codec), "SC_VIDEO_CODEC=%s",
             o->video ? sc_daemon_codec_name(o->video_codec) : "");
    snprintf(e_brate, sizeof(e_brate), "SC_VIDEO_BIT_RATE=%u",
             o->video_bit_rate);
    snprintf(e_mfps, sizeof(e_mfps), "SC_VIDEO_MAX_FPS=%s",
             o->max_fps ? o->max_fps : "");
    snprintf(e_msize, sizeof(e_msize), "SC_VIDEO_MAX_SIZE=%u", o->max_size);
    snprintf(e_enc, sizeof(e_enc), "SC_VIDEO_ENCODER=%s",
             o->video_encoder ? o->video_encoder : "");
    snprintf(e_ctrl, sizeof(e_ctrl), "SC_CONTROL=%d", o->control ? 1 : 0);

    // Result channel: the script writes its {result,reason,thinking,...} JSON to
    // SC_RESULT_FILE and the daemon reads it back into the response.
    int rfd = make_temp_file("result", &result_path);
    if (rfd >= 0) {
        close(rfd); // reserve the path; the script (over)writes it
    }
    char e_result[600];
    snprintf(e_result, sizeof(e_result), "SC_RESULT_FILE=%s",
             result_path ? result_path : "");

    // Base env + result file + the primary as SC_ARG_<COMMAND> + one per arg
    const char *env[16 + 1 + SC_MAX_ADDON_ARGS] = {
        e_port, e_host, e_serial, e_real_udid, e_report, e_name, e_w, e_h,
        e_exe, e_result, e_codec, e_brate, e_mfps, e_msize, e_enc, e_ctrl};
    unsigned env_count = 16;
    char *arg_env[1 + SC_MAX_ADDON_ARGS] = {0};
    unsigned arg_env_count = 0;
    bool env_ok = true;

    char *primary_env = make_arg_env(name, args ? args : "");
    if (primary_env) {
        arg_env[arg_env_count++] = primary_env;
        env[env_count++] = primary_env;
    } else {
        env_ok = false;
    }
    for (unsigned i = 0; i < name_count; ++i) {
        char *e = make_arg_env(arg_names[i], arg_values[i]);
        if (e) {
            arg_env[arg_env_count++] = e;
            env[env_count++] = e;
        } else {
            env_ok = false;
        }
    }

    sc_pid pid = SC_PROCESS_NONE;
    bool session_usable = plugin_copy_may_continue(d);
    bool started =
        env_ok && session_usable
        && sc_addon_run(addon->path, args, env, env_count, &pid);
    free(exe);
    // The env strings were copied into the child at exec time; safe to free now
    for (unsigned i = 0; i < arg_env_count; ++i) {
        free(arg_env[i]);
    }
    if (!started) {
        if (!session_usable) {
            status = "error";
            snprintf(response_error, sizeof(response_error),
                     "device session ended before starting add-on");
        } else {
            snprintf(response_error, sizeof(response_error),
                     "%s", env_ok ? "could not start add-on"
                                  : "could not prepare add-on environment");
        }
        goto finish_result_path;
    }

    // Publish process ownership only while the leased session is still
    // READY. This closes the acquire->fork window where a drain could scan
    // before plugin_pid existed and then wait forever for the new child.
    sc_mutex_lock(&d->mutex);
    bool pid_published =
        d->state == SC_DAEMON_STATE_READY && !d->stop && !d->session.dead;
    if (pid_published) {
        d->conns[conn_index].plugin_pid = pid;
    }
    sc_mutex_unlock(&d->mutex);
    if (!pid_published) {
        sc_exit_code code = SC_EXIT_CODE_NONE;
        enum sc_service_process_state process_state =
            terminate_process_bounded(pid, &code);
        if (process_state == SC_SERVICE_PROCESS_EXITED) {
            sc_process_close(pid);
        } else {
            LOGE("Could not terminate add-on process while session ended");
#ifdef _WIN32
            sc_process_close(pid);
#endif
        }
        if (result_path) {
            result_status =
                sc_plugin_result_read(result_path,
                                      SC_PLUGIN_RESULT_MAX_SIZE, &result);
        }
        has_exit_code = process_state == SC_SERVICE_PROCESS_EXITED
                     && code != SC_EXIT_CODE_NONE;
        if (has_exit_code) {
            exit_code = (int64_t) code;
        }
        status = "error";
        snprintf(response_error, sizeof(response_error),
                 "device session ended while starting add-on");
        goto finish_result_path;
    }

    if (addon->service) {
        struct sc_service_plugin_outcome service =
            wait_and_maybe_adopt_service(d, addon, pid, result_path,
                                         conn_index);
        result = service.result;
        result_status = service.result_status;
        adopted = service.adopted;
        has_exit_code = service.has_exit_code;
        if (has_exit_code) {
            exit_code = (int64_t) service.exit_code;
        }

        if (service.process_error) {
            status = "error";
            snprintf(response_error, sizeof(response_error),
                     "could not observe service add-on process");
        } else if (service.session_ended) {
            status = "error";
            snprintf(response_error, sizeof(response_error),
                     "device session ended before service adoption");
        } else if (service.adopt_failed) {
            status = "adopt_failed";
            snprintf(response_error, sizeof(response_error),
                     "could not adopt service add-on");
        } else if (service.exited) {
            if (service.has_exit_code && service.exit_code == 0) {
                if (result_status == SC_PLUGIN_RESULT_INVALID
                        || result_status == SC_PLUGIN_RESULT_TOO_LARGE
                        || result_status == SC_PLUGIN_RESULT_IO_ERROR) {
                    status = "invalid_result";
                    response_error_code = "E_PLUGIN";
                    snprintf(response_error, sizeof(response_error),
                             "add-on produced an invalid result");
                } else {
                    status = "ok";
                    response_ok = true;
                }
            } else {
                status = "error";
                if (service.has_exit_code) {
                    snprintf(response_error, sizeof(response_error),
                             "add-on exited with code %" PRId64, exit_code);
                } else {
                    snprintf(response_error, sizeof(response_error),
                             "add-on was terminated");
                }
            }
        } else if (service.timed_out) {
            status = "ready_timeout";
            response_ok = true;
        } else {
            assert(service.ready && service.adopted);
            status = "ready";
            response_ok = true;
        }
    } else {
        // Observe termination without reaping/closing until plugin_pid has
        // been cleared under the same mutex used by the drain thread.
        sc_exit_code code = SC_EXIT_CODE_NONE;
        enum sc_service_process_state process_state =
            plugin_wait_for_exit(d, conn_index, pid, &code);
        sc_mutex_lock(&d->mutex);
        d->conns[conn_index].plugin_pid = SC_PROCESS_NONE;
        sc_mutex_unlock(&d->mutex);
        if (process_state == SC_SERVICE_PROCESS_EXITED) {
            sc_process_close(pid);
        } else {
            LOGE("Lost ownership of add-on process \"%s\"", addon->name);
#ifdef _WIN32
            // CloseHandle only releases local ownership and never terminates
            // the underlying process.
            sc_process_close(pid);
#endif
        }

        if (result_path) {
            result_status =
                sc_plugin_result_read(result_path,
                                      SC_PLUGIN_RESULT_MAX_SIZE, &result);
        }
        has_exit_code = process_state == SC_SERVICE_PROCESS_EXITED
                     && code != SC_EXIT_CODE_NONE;
        if (has_exit_code) {
            exit_code = (int64_t) code;
        }
        if (process_state == SC_SERVICE_PROCESS_ERROR) {
            status = "error";
            snprintf(response_error, sizeof(response_error),
                     "could not observe add-on process");
        } else if (has_exit_code && code == 0) {
            if (result_status == SC_PLUGIN_RESULT_INVALID
                    || result_status == SC_PLUGIN_RESULT_TOO_LARGE
                    || result_status == SC_PLUGIN_RESULT_IO_ERROR) {
                status = "invalid_result";
                response_error_code = "E_PLUGIN";
                snprintf(response_error, sizeof(response_error),
                         "add-on produced an invalid result");
            } else {
                status = "ok";
                response_ok = true;
            }
        } else {
            status = "error";
            if (has_exit_code) {
                snprintf(response_error, sizeof(response_error),
                         "add-on exited with code %" PRId64, exit_code);
            } else {
                snprintf(response_error, sizeof(response_error),
                         "add-on was terminated");
            }
        }
    }

finish_result_path:
    if (result_path) {
        unlink(result_path);
        free(result_path);
        result_path = NULL;
    }

    result_json =
        result_status == SC_PLUGIN_RESULT_VALID ? result.json : NULL;

finish_with_assets:
    if (report_timed) {
        if (!sc_report_get_timeline_time_ms(&d->report, &end_ms)) {
            sc_report_mark_failed(&d->report);
        } else {
            struct sc_plugin_named_input named[SC_MAX_ADDON_ARGS];
            for (unsigned i = 0; i < name_count; ++i) {
                named[i].name = arg_names[i];
                named[i].value = arg_values[i];
            }

            struct sc_plugin_completion_event event = {
                .name = name,
                .args = args,
                .named = named,
                .named_count = name_count,
                .assets = (const char *const *) assets.items,
                .asset_count = assets.count,
                .start_ms = start_ms,
                .end_ms = end_ms,
                .duration_ms = end_ms - start_ms,
                .result_json = result_json,
                .status = status,
                .has_exit_code = has_exit_code,
                .exit_code = exit_code,
                .service = addon->service,
                .adopted = adopted,
            };
            char *extra = NULL;
            bool serialized =
                sc_plugin_event_serialize_extra(&event, &extra);
            bool logged = serialized
                       && sc_report_log_event_at(&d->report, end_ms, "plugin",
                                                 NULL, extra);
            free(extra);
            if (!logged) {
                sc_report_mark_failed(&d->report);
            }
        }
    }

finish:
    // Persist the completion event above before allowing session finalization.
    if (acquired) {
        release_session(d);
        acquired = false;
    }

    if (send_response) {
        if (response_ok) {
            if (result_json) {
                struct sc_strbuf eb = {0};
                if (sc_strbuf_init(&eb, 256)
                        && sc_strbuf_append_staticstr(&eb, "\"result\":")
                        && sc_strbuf_append_str(&eb, result_json)) {
                    send_ok(socket, id, eb.s, NULL, 0);
                } else {
                    send_error(socket, id, "E_INTERNAL",
                               "could not serialize add-on result");
                }
                free(eb.s);
            } else {
                send_ok(socket, id, NULL, NULL, 0);
            }
        } else {
            send_error(socket, id, response_error_code, response_error);
        }
    }

cleanup:
    if (result_path) {
        unlink(result_path);
        free(result_path);
    }
    plugin_assets_destroy(&assets);
    sc_plugin_result_destroy(&result);
    for (unsigned i = 0; i < name_count; ++i) {
        free(arg_names[i]);
    }
    for (unsigned i = 0; i < value_count; ++i) {
        free(arg_values[i]);
    }
    free(name);
    free(args);
}

// Receive a file uploaded by a remote client (doc/addons.md §7): the request
// carries "payload_len" and that many raw bytes follow the JSON frame. The
// bytes are stored in a per-connection temp file (removed on disconnect) and
// its daemon-side path is returned, to be passed as a path/pathlist plugin arg.
static void
handle_upload(struct sc_daemon *d, sc_socket socket, int64_t id,
              const struct sc_json *json, unsigned conn_index) {
    int64_t payload_len = 0;
    sc_json_get_int64(json, "payload_len", &payload_len);
    if (payload_len < 0 || (uint64_t) payload_len > SC_DAEMON_MAX_FRAME_SIZE) {
        send_error(socket, id, "E_BAD_REQUEST", "invalid upload payload_len");
        return;
    }

    uint8_t *data = NULL;
    if (payload_len > 0
            && !sc_daemon_read_payload(socket, (size_t) payload_len, &data)) {
        // The stream is now desynchronized; closing follows on the next read
        send_error(socket, id, "E_BAD_REQUEST", "could not read upload payload");
        return;
    }

    char *path = NULL;
    int fd = make_temp_file("upload", &path);
    if (fd < 0) {
        free(data);
        send_error(socket, id, "E_INTERNAL", "could not create upload file");
        return;
    }
    bool ok = payload_len == 0 || write_all_fd(fd, data, (size_t) payload_len);
    close(fd);
    free(data);
    if (!ok) {
        unlink(path);
        free(path);
        send_error(socket, id, "E_INTERNAL", "could not write upload file");
        return;
    }

    // Track for cleanup when the connection closes
    sc_mutex_lock(&d->mutex);
    struct sc_daemon_conn *conn = &d->conns[conn_index];
    bool tracked = conn->upload_count < SC_DAEMON_MAX_UPLOADS;
    if (tracked) {
        conn->uploads[conn->upload_count++] = path;
    }
    sc_mutex_unlock(&d->mutex);
    if (!tracked) {
        unlink(path);
        free(path);
        send_error(socket, id, "E_BAD_REQUEST", "too many uploads");
        return;
    }

    struct sc_strbuf eb;
    if (!sc_strbuf_init(&eb, 128)) {
        send_error(socket, id, "E_INTERNAL", "out of memory");
        return; // path stays tracked and will be cleaned on disconnect
    }
    if (sc_strbuf_append_staticstr(&eb, "\"path\":")
            && sc_json_append_escaped(&eb, path)) {
        send_ok(socket, id, eb.s, NULL, 0);
    } else {
        send_error(socket, id, "E_INTERNAL", "out of memory");
    }
    free(eb.s);
}

enum sc_conn_action {
    SC_CONN_CONTINUE,      // keep reading requests
    SC_CONN_CLOSE,         // close the connection
    SC_CONN_SUBSCRIBE_VIDEO, // hand the connection to the video broadcaster
};

// Handle one request
static enum sc_conn_action
handle_request(struct sc_daemon *d, sc_socket socket, const char *json_str,
               size_t json_len, unsigned conn_index) {
    struct sc_json json;
    if (!sc_json_parse(&json, json_str, json_len)) {
        send_error(socket, 0, "E_BAD_REQUEST", "invalid JSON");
        return SC_CONN_CONTINUE;
    }

    int64_t id = 0;
    sc_json_get_int64(&json, "id", &id);

    char *op;
    if (!sc_json_get_string(&json, "op", &op)) {
        send_error(socket, id, "E_BAD_REQUEST", "missing \"op\"");
        return SC_CONN_CONTINUE;
    }

    enum sc_conn_action action = SC_CONN_CONTINUE;

    if (!strcmp(op, "ping")) {
        send_ok(socket, id, NULL, NULL, 0);
    } else if (!strcmp(op, "status")) {
        handle_status(d, socket, id);
    } else if (!strcmp(op, "screencap")) {
        handle_screencap(d, socket, id, &json);
    } else if (!strcmp(op, "clip")) {
        handle_clip(d, socket, id, &json);
    } else if (!strcmp(op, "control")) {
        handle_control(d, socket, id, &json, conn_index);
    } else if (!strcmp(op, "inject_touch")) {
        handle_inject_touch(d, socket, id, &json, conn_index);
    } else if (!strcmp(op, "inject_key")) {
        handle_inject_key(d, socket, id, &json);
    } else if (!strcmp(op, "inject_text")) {
        handle_inject_text(d, socket, id, &json);
    } else if (!strcmp(op, "inject_scroll")) {
        handle_inject_scroll(d, socket, id, &json);
    } else if (!strcmp(op, "camera_set_torch")) {
        handle_camera_set_torch(d, socket, id, &json);
    } else if (!strcmp(op, "camera_zoom_in")) {
        handle_camera_zoom(d, socket, id, true);
    } else if (!strcmp(op, "camera_zoom_out")) {
        handle_camera_zoom(d, socket, id, false);
    } else if (!strcmp(op, "resize_display")) {
        handle_resize_display(d, socket, id, &json);
    } else if (!strcmp(op, "expand_notification_panel")) {
        handle_device_command(d, socket, id,
                              SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
                              op, true);
    } else if (!strcmp(op, "expand_settings_panel")) {
        handle_device_command(d, socket, id,
                              SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL,
                              op, true);
    } else if (!strcmp(op, "collapse_panels")) {
        handle_device_command(d, socket, id,
                              SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS,
                              op, true);
    } else if (!strcmp(op, "rotate_device")) {
        handle_device_command(d, socket, id,
                              SC_CONTROL_MSG_TYPE_ROTATE_DEVICE, op, true);
    } else if (!strcmp(op, "open_hard_keyboard_settings")) {
        handle_device_command(
            d, socket, id,
            SC_CONTROL_MSG_TYPE_OPEN_HARD_KEYBOARD_SETTINGS, op, true);
    } else if (!strcmp(op, "set_display_power")) {
        handle_set_display_power(d, socket, id, &json);
    } else if (!strcmp(op, "reset_video")) {
        handle_device_command(d, socket, id,
                              SC_CONTROL_MSG_TYPE_RESET_VIDEO, op, false);
    } else if (!strcmp(op, "note")) {
        handle_note(d, socket, id, &json);
    } else if (!strcmp(op, "plugin")) {
        handle_plugin(d, socket, id, &json, conn_index);
    } else if (!strcmp(op, "upload")) {
        handle_upload(d, socket, id, &json, conn_index);
    } else if (!strcmp(op, "subscribe_video")) {
        if (!d->opts->video) {
            send_error(socket, id, "E_BAD_REQUEST",
                       "video is disabled (--no-video)");
        } else {
            // Ready check only; the broadcaster sends video_meta as the ack
            sc_mutex_lock(&d->mutex);
            bool ready = d->state == SC_DAEMON_STATE_READY && !d->stop
                      && !d->session.dead;
            sc_mutex_unlock(&d->mutex);
            if (ready) {
                action = SC_CONN_SUBSCRIBE_VIDEO;
            } else {
                send_error(socket, id, "E_NOT_READY", "session not ready");
            }
        }
    } else if (!strcmp(op, "shutdown")) {
        send_ok(socket, id, NULL, NULL, 0);
        LOGI("Daemon: shutdown requested by client");
        sc_mutex_lock(&d->mutex);
        d->stop = true;
        sc_cond_broadcast(&d->cond);
        sc_mutex_unlock(&d->mutex);
        action = SC_CONN_CLOSE;
    } else {
        send_error(socket, id, "E_BAD_REQUEST", "unknown op");
    }

    free(op);
    return action;
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
            enum sc_conn_action action =
                handle_request(d, socket, json, len, conn_index);
            free(json);
            if (action == SC_CONN_CLOSE) {
                break;
            }
            if (action == SC_CONN_SUBSCRIBE_VIDEO) {
                // Hand this connection to the video broadcaster. This
                // connection thread drains its own bounded queue; the device
                // demuxer never performs socket I/O and therefore cannot be
                // stalled by a slow or abandoned browser.
                if (!sc_broadcaster_subscribe(&d->broadcaster, socket)) {
                    break;
                }
                // The broadcaster caches the latest config/keyframe, so a new
                // subscriber starts decodably without RESET_VIDEO. Opening or
                // refreshing a page must not restart the encoder or split the
                // recording timeline.
                sc_broadcaster_run(&d->broadcaster, socket);
                sc_broadcaster_unsubscribe(&d->broadcaster, socket);
                break;
            }
        }
    }

    sc_mutex_lock(&d->mutex);
    // Remove any files this connection uploaded (doc/addons.md §7)
    for (unsigned i = 0; i < conn->upload_count; ++i) {
        unlink(conn->uploads[i]);
        free(conn->uploads[i]);
        conn->uploads[i] = NULL;
    }
    conn->upload_count = 0;
    // Publish completion before closing the macOS socket wrapper. Shutdown
    // checks this flag under the same mutex before calling net_interrupt();
    // closing first would leave a window where it dereferences freed memory.
    conn->finished = true;
    sc_mutex_unlock(&d->mutex);

    net_close(socket);
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
        conn->upload_count = 0;
        sc_touch_report_init(&conn->touch_report);
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
sc_daemon_wait_session_end(struct sc_daemon *d, sc_tick deadline) {
    sc_mutex_lock(&d->mutex);
    while (!d->stop && !d->session.dead) {
        if (g_stop_signal) {
            d->stop = true;
            break;
        }
        sc_tick now = sc_tick_now();
        if (deadline && now >= deadline) {
            LOGI("Daemon: time limit reached");
            d->stop = true;
            break;
        }
        sc_tick wait_deadline = now + SC_DAEMON_WAIT_TICK;
        if (deadline && wait_deadline > deadline) {
            wait_deadline = deadline;
        }
        sc_cond_timedwait(&d->cond, &d->mutex, wait_deadline);
    }
    sc_mutex_unlock(&d->mutex);
}

// Block new requests and wait for in-flight requests to complete
static void
sc_daemon_drain_requests(struct sc_daemon *d, enum sc_daemon_state state) {
    sc_mutex_lock(&d->mutex);
    sc_daemon_set_state_locked(d, state);
    // A plugin request holds a session lease until its single completion
    // event is persisted. Terminate children that are still running so a
    // device disconnect or daemon stop cannot wait forever before closing the
    // report gate. The waiting connection thread reaps the child, logs the
    // failure outcome, then releases its lease.
    while (d->in_flight) {
        // Repeat on every wake as a defensive fallback for a child published
        // at the READY->draining boundary.
        for (unsigned i = 0; i < SC_DAEMON_MAX_CLIENTS; ++i) {
            sc_pid pid = d->conns[i].plugin_pid;
            if (pid != SC_PROCESS_NONE) {
                sc_process_terminate(pid);
            }
        }
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
    d->report_active = opts->auto_test_report != NULL;
    atomic_init(&d->report_initialized, false);
    atomic_init(&d->report_recording, false);

    for (unsigned i = 0; i < SC_DAEMON_MAX_CLIENTS; ++i) {
        d->conns[i].plugin_pid = SC_PROCESS_NONE;
        d->conns[i].upload_count = 0;
    }
    sc_addons_load(&d->addons, opts->add_ons, opts->add_on_count);

    bool mutex_ok = false;
    bool cond_ok = false;
    bool clip_ok = false;
    bool keeper_ok = false;
    bool broadcaster_ok = false;
    bool clips_ok = false;
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
    if (!sc_broadcaster_init(&d->broadcaster)) {
        goto end;
    }
    broadcaster_ok = true;
    if (!sc_clip_buffer_init(&d->clips, &d->keeper)) {
        goto end;
    }
    clips_ok = true;

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
            if (d->report_initialized) {
                // A report is a single immutable session. Once its files have
                // been opened (and possibly finalized on a partial startup),
                // retrying would mix device identities or silently drop
                // events behind a closed finalization gate.
                LOGE("Daemon: report session failed before becoming ready; "
                     "not retrying");
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

        sc_tick deadline = opts->time_limit
                         ? sc_tick_now() + opts->time_limit
                         : 0;
        sc_daemon_wait_session_end(d, deadline);

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
        if (d->report_active) {
            // A test report captures a single session: end the test on the
            // first disconnect (decision: --auto-test-report ends immediately)
            LOGI("Daemon: test report ended (device disconnected)");
            ret = SCRCPY_EXIT_SUCCESS;
            break;
        }
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

    // Wake subscription writers before joining client connections. They may
    // be blocked in a socket send or waiting for their next queued frame.
    if (broadcaster_ok) {
        sc_broadcaster_interrupt_all(&d->broadcaster);
    }

    // Interrupt and join client connections
    if (mutex_ok) {
        sc_mutex_lock(&d->mutex);
        for (unsigned i = 0; i < SC_DAEMON_MAX_CLIENTS; ++i) {
            struct sc_daemon_conn *conn = &d->conns[i];
            if (conn->in_use && !conn->finished) {
                net_interrupt(conn->socket);
            }
            // Terminate a running add-on so its blocking wait returns
            if (conn->plugin_pid != SC_PROCESS_NONE) {
                sc_process_terminate(conn->plugin_pid);
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

    // Terminate adopted long-running service add-ons (doc/addons.md)
    for (unsigned i = 0; i < d->service_count; ++i) {
        if (d->services[i].pid != SC_PROCESS_NONE) {
            LOGI("Stopping service add-on \"%s\"", d->services[i].name);
            terminate_service(d->services[i].pid);
        }
        free(d->services[i].name);
    }
    d->service_count = 0;

    if (listening) {
        net_close(d->listen_socket);
    }
    if (registry_written) {
        sc_registry_remove(opts->daemon_port);
    }
    if (clips_ok) {
        sc_clip_buffer_destroy(&d->clips);
    }
    if (broadcaster_ok) {
        sc_broadcaster_destroy(&d->broadcaster);
    }
    sc_addons_destroy(&d->addons);
    if (d->report_initialized) {
        sc_report_destroy(&d->report);
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
