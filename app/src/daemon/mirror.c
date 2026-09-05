#include "mirror.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "control_msg.h"
#include "controller.h"
#include "decoder.h"
#include "demuxer.h"
#include "events.h"
#include "keyboard_sdk.h"
#include "mouse_sdk.h"
#include "screen.h"
#include "sdl_hints.h"
#include "daemon/client.h"
#include "daemon/codec.h"
#include "daemon/protocol.h"
#include "util/binary.h"
#include "util/log.h"
#include "util/net.h"
#include "util/str.h"
#include "util/strbuf.h"
#include "util/term.h"
#include "util/thread.h"
#include "util/tick.h"
#include "util/timeout.h"
#include "video_regulator.h"

// Demuxer wire format constants (app/src/demuxer.c; the adapter thread
// produces this format so the stock demuxer consumes the daemon stream)
#define SC_MIRROR_PACKET_HEADER_SIZE 12
#define SC_MIRROR_PACKET_FLAG_CONFIG    (UINT64_C(1) << 62)
#define SC_MIRROR_PACKET_FLAG_KEY_FRAME (UINT64_C(1) << 61)
#define SC_MIRROR_PTS_MASK (SC_MIRROR_PACKET_FLAG_KEY_FRAME - 1)

struct sc_mirror {
    // Two daemon connections, like scrcpy-auto-web: one becomes the one-way
    // subscribe_video push stream, the other carries inject_* requests
    sc_socket video_conn;
    sc_socket ctrl_conn;

    // Loopback pairs so the STOCK demuxer and controller are reused unchanged:
    // the video adapter translates daemon frames into the demuxer wire format,
    // the control adapter parses the controller's serialized messages back
    // into daemon inject_* requests
    sc_socket video_pair_read;  // demuxer reads from here
    sc_socket video_pair_write; // video adapter writes here
    sc_socket ctrl_pair_read;   // control adapter reads from here
    sc_socket ctrl_pair_write;  // controller writes here

    sc_thread video_thread;
    sc_thread ctrl_thread;
};

// Connect both ends of a local TCP pair (portable socketpair substitute)
static bool
mirror_socketpair(sc_socket *read_end, sc_socket *write_end) {
    bool ok = false;
    sc_socket client = SC_SOCKET_NONE;
    sc_socket server = SC_SOCKET_NONE;

    sc_socket listener = net_socket();
    if (listener == SC_SOCKET_NONE) {
        return false;
    }
    if (!net_listen(listener, IPV4_LOCALHOST, 0, 1)) {
        goto end;
    }
    uint16_t local_port = net_local_port(listener);
    if (!local_port) {
        goto end;
    }
    client = net_socket();
    if (client == SC_SOCKET_NONE) {
        goto end;
    }
    if (!net_connect(client, IPV4_LOCALHOST, local_port)) {
        goto end;
    }
    server = net_accept(listener);
    if (server == SC_SOCKET_NONE) {
        goto end;
    }
    net_set_tcp_nodelay(client, true);
    net_set_tcp_nodelay(server, true);
    *read_end = server;
    *write_end = client;
    ok = true;
end:
    if (!ok) {
        if (client != SC_SOCKET_NONE) {
            net_close(client);
        }
        if (server != SC_SOCKET_NONE) {
            net_close(server);
        }
    }
    net_close(listener);
    return ok;
}

// Resolve --daemon-host to an IPv4 address (defaults to loopback)
static bool
mirror_resolve_host(const char *host, uint32_t *addr) {
    if (!host || !*host || !strcmp(host, "localhost")) {
        *addr = IPV4_LOCALHOST;
        return true;
    }
    if (!net_parse_ipv4(host, addr)) {
        LOGE("Mirror: --daemon-host must be an IPv4 address or \"localhost\" "
             "(got \"%s\")", host);
        return false;
    }
    return true;
}

// Connect to the daemon and consume its hello; returns the socket and,
// optionally, the advertised device name (malloc'd)
static sc_socket
mirror_connect(uint32_t addr, uint16_t port, char **out_device_name,
               char **out_state, char **out_video_source,
               bool *out_flex_display) {
    sc_socket sock = net_socket();
    if (sock == SC_SOCKET_NONE) {
        return SC_SOCKET_NONE;
    }
    if (!net_connect(sock, addr, port)) {
        LOGE("Mirror: could not connect to daemon on port %" PRIu16
             " (is it running?)", port);
        net_close(sock);
        return SC_SOCKET_NONE;
    }
    net_set_tcp_nodelay(sock, true);

    // The daemon speaks first (doc/daemon.md §8.2)
    struct sc_json hello;
    char *doc;
    size_t doc_len;
    if (!sc_daemon_read_json(sock, &doc, &doc_len)) {
        LOGE("Mirror: connection to daemon lost");
        net_close(sock);
        return SC_SOCKET_NONE;
    }
    if (!sc_json_parse(&hello, doc, doc_len)) {
        LOGE("Mirror: invalid hello from daemon");
        free(doc);
        net_close(sock);
        return SC_SOCKET_NONE;
    }
    int64_t protocol = 0;
    bool proto_ok = sc_json_get_int64(&hello, "protocol", &protocol)
                 && protocol == SC_DAEMON_PROTOCOL_VERSION;
    if (!proto_ok) {
        LOGE("Mirror: daemon protocol mismatch (daemon: %" PRId64
             ", client: %d); update scrcpy-auto on both sides",
             protocol, SC_DAEMON_PROTOCOL_VERSION);
        free(doc);
        net_close(sock);
        return SC_SOCKET_NONE;
    }
    if (out_device_name) {
        *out_device_name = NULL;
        sc_json_get_string(&hello, "device_name", out_device_name);
    }
    if (out_state) {
        *out_state = NULL;
        sc_json_get_string(&hello, "state", out_state);
    }
    if (out_video_source) {
        *out_video_source = NULL;
        sc_json_get_string(&hello, "video_source", out_video_source);
    }
    if (out_flex_display) {
        *out_flex_display = false;
        sc_json_get_bool(&hello, "flex_display", out_flex_display);
    }
    free(doc);
    return sock;
}

// ---- video adapter: daemon subscribe_video events -> demuxer wire format ---

static bool
mirror_send_session(sc_socket sock, uint32_t width, uint32_t height,
                    bool client_resized) {
    uint8_t header[SC_MIRROR_PACKET_HEADER_SIZE] = {0};
    header[0] = 0x80; // session packet flag
    header[3] = client_resized ? 1 : 0;
    sc_write32be(&header[4], width);
    sc_write32be(&header[8], height);
    return net_send_all(sock, header, sizeof(header)) == sizeof(header);
}

static bool
mirror_send_packet(sc_socket sock, uint64_t pts_flags, const uint8_t *data,
                   size_t len) {
    uint8_t header[SC_MIRROR_PACKET_HEADER_SIZE];
    sc_write64be(header, pts_flags);
    sc_write32be(&header[8], (uint32_t) len);
    if (net_send_all(sock, header, sizeof(header)) != sizeof(header)) {
        return false;
    }
    return net_send_all(sock, data, len) == (ssize_t) len;
}

struct sc_mirror_video_ctx {
    struct sc_mirror *mirror;
};

static int
run_video_adapter(void *data) {
    struct sc_mirror *mirror = data;

    bool codec_sent = false;
    uint32_t codec_id = 0;
    sc_tick first_pts = 0;

    for (;;) {
        char *doc;
        size_t doc_len;
        if (!sc_daemon_read_json(mirror->video_conn, &doc, &doc_len)) {
            break; // daemon disconnected (or interrupted for shutdown)
        }
        struct sc_json json;
        if (!sc_json_parse(&json, doc, doc_len)) {
            LOGE("Mirror: invalid frame from daemon");
            free(doc);
            break;
        }

        char *event = NULL;
        if (!sc_json_get_string(&json, "event", &event)) {
            bool response_ok;
            if (sc_json_get_bool(&json, "ok", &response_ok)
                    && !response_ok) {
                // subscribe_video returns an ordinary error response while
                // the daemon is connecting/reconnecting. Continuing to wait
                // on this request socket would leave a permanently blank
                // mirror because it was never promoted to a subscription.
                LOGE("Mirror: video subscription rejected: %s", doc);
                free(doc);
                break;
            }
            free(doc);
            continue; // unrelated response
        }

        bool ok = true;
        if (!strcmp(event, "video_meta")) {
            char *codec = NULL;
            int64_t width = 0;
            int64_t height = 0;
            bool client_resized = false;
            sc_json_get_string(&json, "codec", &codec);
            sc_json_get_int64(&json, "width", &width);
            sc_json_get_int64(&json, "height", &height);
            sc_json_get_bool(&json, "client_resized", &client_resized);
            uint32_t id = sc_daemon_codec_id_from_name(codec);
            if (!id) {
                LOGE("Mirror: unsupported codec from daemon: %s",
                     codec ? codec : "(none)");
                ok = false;
            } else if (width <= 0 || width > UINT32_MAX
                    || height <= 0 || height > UINT32_MAX) {
                LOGE("Mirror: invalid video geometry from daemon");
                ok = false;
            } else if (!codec_sent) {
                uint8_t raw[4];
                sc_write32be(raw, id);
                codec_id = id;
                codec_sent =
                    net_send_all(mirror->video_pair_write, raw, 4) == 4;
                ok = codec_sent;
            } else if (id != codec_id) {
                // The demuxer decoder cannot be re-created mid-stream
                LOGE("Mirror: daemon switched codec mid-stream (%s)", codec);
                ok = false;
            }
            if (ok) {
                LOGI("Mirror: video stream: %s %" PRId64 "x%" PRId64,
                     codec ? codec : "h264", width, height);
                ok = mirror_send_session(mirror->video_pair_write,
                                         (uint32_t) width, (uint32_t) height,
                                         client_resized);
            }
            free(codec);
        } else if (!strcmp(event, "video")) {
            int64_t payload_len = 0;
            sc_json_get_int64(&json, "payload_len", &payload_len);
            if (payload_len <= 0
                    || payload_len > SC_DAEMON_MAX_FRAME_SIZE) {
                LOGE("Mirror: invalid video payload length");
                ok = false;
            } else {
                uint8_t *payload;
                ok = sc_daemon_read_payload(mirror->video_conn,
                                            (size_t) payload_len, &payload);
                if (ok) {
                    if (codec_sent) {
                        bool config = false;
                        bool key = false;
                        sc_json_get_bool(&json, "config", &config);
                        sc_json_get_bool(&json, "key", &key);
                        uint64_t pts_flags;
                        if (config) {
                            pts_flags = SC_MIRROR_PACKET_FLAG_CONFIG;
                        } else {
                            // The daemon does not forward device PTS; the
                            // stream is rendered live, so a monotonic
                            // client-side clock is sufficient
                            sc_tick now = sc_tick_now();
                            if (!first_pts) {
                                first_pts = now;
                            }
                            pts_flags = (uint64_t) (now - first_pts)
                                      & SC_MIRROR_PTS_MASK;
                            if (key) {
                                pts_flags |= SC_MIRROR_PACKET_FLAG_KEY_FRAME;
                            }
                        }
                        ok = mirror_send_packet(mirror->video_pair_write,
                                                pts_flags, payload,
                                                (size_t) payload_len);
                    }
                    free(payload);
                }
            }
        }
        // other events (pong, ...) are ignored

        free(event);
        free(doc);
        if (!ok) {
            break;
        }
    }

    // Signal end-of-stream to the demuxer (it reports EOS -> the event loop
    // exits with "device disconnected")
    net_interrupt(mirror->video_pair_write);
    return 0;
}

// ---- control adapter: serialized control msgs -> daemon inject_* requests --

// The serialized layouts below mirror control_msg.c (sc_control_msg_serialize)
// and are version-locked to it: REVIEW ON EVERY UPSTREAM MERGE (doc/fork.md).

static bool
ctrl_read(sc_socket sock, void *buf, size_t len) {
    return net_recv_all(sock, buf, len) == (ssize_t) len;
}

static bool
ctrl_skip(sc_socket sock, size_t len) {
    uint8_t chunk[4096];
    while (len) {
        size_t n = len < sizeof(chunk) ? len : sizeof(chunk);
        if (!ctrl_read(sock, chunk, n)) {
            return false;
        }
        len -= n;
    }
    return true;
}

static const char *
motion_action_str(uint8_t action) {
    switch (action) {
        case 0: // AMOTION_EVENT_ACTION_DOWN
            return "down";
        case 1: // AMOTION_EVENT_ACTION_UP
            return "up";
        case 2: // AMOTION_EVENT_ACTION_MOVE
            return "move";
        case 7: // AMOTION_EVENT_ACTION_HOVER_MOVE
            return "hover_move";
        default:
            return NULL;
    }
}

// Send one request and consume its response; input injection failures are
// reported once (expected on iOS bridges, which are view-only)
static bool
ctrl_request(struct sc_mirror *mirror, const char *json, size_t len,
             int64_t *id, bool *rejected_logged) {
    if (!sc_daemon_write_frame(mirror->ctrl_conn, json, len, NULL, 0)) {
        return false;
    }
    ++*id;

    char *doc;
    size_t doc_len;
    if (!sc_daemon_read_json(mirror->ctrl_conn, &doc, &doc_len)) {
        return false;
    }
    struct sc_json resp;
    bool ok_field;
    if (sc_json_parse(&resp, doc, doc_len)
            && sc_json_get_bool(&resp, "ok", &ok_field) && !ok_field
            && !*rejected_logged) {
        *rejected_logged = true;
        LOGW("Mirror: input rejected by the daemon (view-only stream?): %s",
             doc);
    }
    free(doc);
    return true;
}

// i16 fixed-point -> daemon integer scroll amount (see control_msg.c: values
// were normalized from [-16, 16] to [-1, 1] then scaled by 2^15)
static int32_t
scroll_amount(int16_t i16fp) {
    float value = (float) i16fp / 2048.0f;
    if (value > 0) {
        return value < 1.0f ? 1 : (int32_t) value;
    }
    if (value < 0) {
        return value > -1.0f ? -1 : (int32_t) value;
    }
    return 0;
}

static int
run_ctrl_adapter(void *data) {
    struct sc_mirror *mirror = data;

    int64_t id = 1;
    bool rejected_logged = false;
    bool unknown_logged = false;
    bool drain_only = false;

    for (;;) {
        uint8_t type;
        if (!ctrl_read(mirror->ctrl_pair_read, &type, 1)) {
            break; // controller stopped
        }

        // Fixed part length after the type byte, per message type
        uint8_t buf[40];
        char json[256];
        int json_len = -1; // < 0: nothing to send
        bool parse_ok = true;

        switch (type) {
            case SC_CONTROL_MSG_TYPE_INJECT_KEYCODE: {
                parse_ok = ctrl_read(mirror->ctrl_pair_read, buf, 13);
                if (!parse_ok || drain_only) {
                    break;
                }
                uint8_t action = buf[0];
                uint32_t keycode = sc_read32be(&buf[1]);
                uint32_t repeat = sc_read32be(&buf[5]);
                uint32_t metastate = sc_read32be(&buf[9]);
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"inject_key\","
                    "\"action\":\"%s\",\"keycode\":%" PRIu32
                    ",\"metastate\":%" PRIu32 ",\"repeat\":%" PRIu32 "}",
                    id, action ? "up" : "down", keycode, metastate, repeat);
                break;
            }
            case SC_CONTROL_MSG_TYPE_INJECT_TEXT: {
                uint8_t lenbuf[4];
                parse_ok = ctrl_read(mirror->ctrl_pair_read, lenbuf, 4);
                if (!parse_ok) {
                    break;
                }
                uint32_t len = sc_read32be(lenbuf);
                if (len > SC_DAEMON_MAX_FRAME_SIZE) {
                    parse_ok = false;
                    break;
                }
                char *text = malloc(len + 1);
                if (!text) {
                    parse_ok = false;
                    break;
                }
                parse_ok = ctrl_read(mirror->ctrl_pair_read, text, len);
                if (parse_ok && !drain_only) {
                    text[len] = '\0';
                    struct sc_strbuf sb;
                    if (sc_strbuf_init(&sb, 64 + len)) {
                        char head[48];
                        snprintf(head, sizeof(head),
                                 "{\"id\":%" PRId64
                                 ",\"op\":\"inject_text\",\"text\":", id);
                        bool w = sc_strbuf_append_str(&sb, head)
                              && sc_json_append_escaped(&sb, text)
                              && sc_strbuf_append_char(&sb, '}');
                        if (w) {
                            parse_ok = ctrl_request(mirror, sb.s, sb.len, &id,
                                                    &rejected_logged);
                        }
                        free(sb.s);
                    }
                }
                free(text);
                break;
            }
            case SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT: {
                parse_ok = ctrl_read(mirror->ctrl_pair_read, buf, 31);
                if (!parse_ok || drain_only) {
                    break;
                }
                uint8_t action = buf[0];
                int64_t pointer_id = (int64_t) sc_read64be(&buf[1]);
                uint32_t x = sc_read32be(&buf[9]);
                uint32_t y = sc_read32be(&buf[13]);
                // screen size (buf[17..20]) is stamped daemon-side
                uint16_t pressure_u16 = sc_read16be(&buf[21]);
                uint32_t action_button = sc_read32be(&buf[23]);
                uint32_t buttons = sc_read32be(&buf[27]);
                const char *action_str = motion_action_str(action);
                if (!action_str) {
                    break;
                }
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"inject_touch\","
                    "\"action\":\"%s\",\"x\":%" PRIu32 ",\"y\":%" PRIu32
                    ",\"pointer_id\":%" PRId64
                    ",\"pressure_u16\":%" PRIu16
                    ",\"action_button\":%" PRIu32
                    ",\"buttons\":%" PRIu32 "}",
                    id, action_str, x, y, pointer_id, pressure_u16,
                    action_button, buttons);
                break;
            }
            case SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT: {
                parse_ok = ctrl_read(mirror->ctrl_pair_read, buf, 20);
                if (!parse_ok || drain_only) {
                    break;
                }
                uint32_t x = sc_read32be(&buf[0]);
                uint32_t y = sc_read32be(&buf[4]);
                int16_t hscroll = (int16_t) sc_read16be(&buf[12]);
                int16_t vscroll = (int16_t) sc_read16be(&buf[14]);
                uint32_t buttons = sc_read32be(&buf[16]);
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"inject_scroll\","
                    "\"x\":%" PRIu32 ",\"y\":%" PRIu32
                    ",\"hscroll\":%" PRId32 ",\"vscroll\":%" PRId32
                    ",\"buttons\":%" PRIu32 "}",
                    id, x, y, scroll_amount(hscroll), scroll_amount(vscroll),
                    buttons);
                break;
            }
            case SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON: {
                parse_ok = ctrl_read(mirror->ctrl_pair_read, buf, 1);
                if (!parse_ok || drain_only) {
                    break;
                }
                // AKEYCODE_BACK; the screen-on nuance needs the device session
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"inject_key\","
                    "\"action\":\"%s\",\"keycode\":4}",
                    id, buf[0] ? "up" : "down");
                break;
            }
            // Messages without a daemon equivalent: consume and drop
            case SC_CONTROL_MSG_TYPE_GET_CLIPBOARD:
                parse_ok = ctrl_skip(mirror->ctrl_pair_read, 1);
                break;
            case SC_CONTROL_MSG_TYPE_SET_DISPLAY_POWER:
                parse_ok = ctrl_read(mirror->ctrl_pair_read, buf, 1);
                if (parse_ok && !drain_only) {
                    json_len = snprintf(json, sizeof(json),
                        "{\"id\":%" PRId64
                        ",\"op\":\"set_display_power\",\"on\":%s}",
                        id, buf[0] ? "true" : "false");
                }
                break;
            case SC_CONTROL_MSG_TYPE_CAMERA_SET_TORCH:
                parse_ok = ctrl_read(mirror->ctrl_pair_read, buf, 1);
                if (parse_ok && !drain_only) {
                    json_len = snprintf(json, sizeof(json),
                        "{\"id\":%" PRId64 ",\"op\":\"camera_set_torch\","
                        "\"on\":%s}", id, buf[0] ? "true" : "false");
                }
                break;
            case SC_CONTROL_MSG_TYPE_SET_CLIPBOARD: {
                uint8_t head[13]; // sequence(8) + paste(1) + length(4)
                parse_ok = ctrl_read(mirror->ctrl_pair_read, head, 13)
                        && ctrl_skip(mirror->ctrl_pair_read,
                                     sc_read32be(&head[9]));
                break;
            }
            case SC_CONTROL_MSG_TYPE_START_APP: {
                uint8_t len;
                parse_ok = ctrl_read(mirror->ctrl_pair_read, &len, 1)
                        && ctrl_skip(mirror->ctrl_pair_read, len);
                break;
            }
            case SC_CONTROL_MSG_TYPE_SCAN_FILE: {
                // scrcpy 4.1 added this variable-length control message. A
                // mirror client has no local file pusher, so there is nothing
                // to scan daemon-side, but it must still consume the complete
                // message to keep the version-locked stream synchronized.
                uint8_t lenbuf[4];
                parse_ok = ctrl_read(mirror->ctrl_pair_read, lenbuf, 4)
                        && ctrl_skip(mirror->ctrl_pair_read,
                                     sc_read32be(lenbuf));
                break;
            }
            case SC_CONTROL_MSG_TYPE_RESIZE_DISPLAY:
                parse_ok = ctrl_read(mirror->ctrl_pair_read, buf, 4);
                if (parse_ok && !drain_only) {
                    json_len = snprintf(json, sizeof(json),
                        "{\"id\":%" PRId64 ",\"op\":\"resize_display\","
                        "\"width\":%u,\"height\":%u}", id,
                        sc_read16be(buf), sc_read16be(&buf[2]));
                }
                break;
            case SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL:
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64
                    ",\"op\":\"expand_notification_panel\"}", id);
                break;
            case SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL:
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64
                    ",\"op\":\"expand_settings_panel\"}", id);
                break;
            case SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS:
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"collapse_panels\"}", id);
                break;
            case SC_CONTROL_MSG_TYPE_ROTATE_DEVICE:
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"rotate_device\"}", id);
                break;
            case SC_CONTROL_MSG_TYPE_OPEN_HARD_KEYBOARD_SETTINGS:
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64
                    ",\"op\":\"open_hard_keyboard_settings\"}", id);
                break;
            case SC_CONTROL_MSG_TYPE_RESET_VIDEO:
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"reset_video\"}", id);
                break;
            case SC_CONTROL_MSG_TYPE_CAMERA_ZOOM_IN:
            case SC_CONTROL_MSG_TYPE_CAMERA_ZOOM_OUT:
                json_len = snprintf(json, sizeof(json),
                    "{\"id\":%" PRId64 ",\"op\":\"%s\"}", id,
                    type == SC_CONTROL_MSG_TYPE_CAMERA_ZOOM_IN
                        ? "camera_zoom_in" : "camera_zoom_out");
                break;
            default:
                // Unknown message: the stream cannot be re-synchronized.
                // Keep draining so the controller never blocks, but stop
                // forwarding.
                if (!unknown_logged) {
                    unknown_logged = true;
                    LOGW("Mirror: unknown control message type %" PRIu8
                         "; input forwarding disabled", type);
                }
                drain_only = true;
                continue;
        }

        if (!parse_ok) {
            break;
        }
        if (json_len > 0 && !drain_only) {
            assert((size_t) json_len < sizeof(json));
            if (!ctrl_request(mirror, json, (size_t) json_len, &id,
                              &rejected_logged)) {
                // The daemon connection is gone; keep draining the controller
                // so it never blocks, video may still be running
                drain_only = true;
            }
        }
    }

    return 0;
}

// ---- pipeline callbacks (same policy as scrcpy.c) ----

static void
mirror_demuxer_on_ended(struct sc_demuxer *demuxer,
                        enum sc_demuxer_status status, void *userdata) {
    (void) demuxer;
    (void) userdata;

    if (status == SC_DEMUXER_STATUS_EOS) {
        sc_push_event(SC_EVENT_DEVICE_DISCONNECTED);
    } else {
        sc_push_event(SC_EVENT_DEMUXER_ERROR);
    }
}

static void
mirror_controller_on_ended(struct sc_controller *controller, bool error,
                           void *userdata) {
    (void) controller;
    (void) userdata;

    if (error) {
        sc_push_event(SC_EVENT_CONTROLLER_ERROR);
    } else {
        sc_push_event(SC_EVENT_DEVICE_DISCONNECTED);
    }
}

static enum scrcpy_exit_code
mirror_event_loop(struct sc_screen *screen) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SC_EVENT_DEVICE_DISCONNECTED:
                LOGW("Daemon stream ended");
                sc_screen_handle_event(screen, &event);
                return SCRCPY_EXIT_DISCONNECTED;
            case SC_EVENT_DEMUXER_ERROR:
                LOGE("Demuxer error");
                return SCRCPY_EXIT_FAILURE;
            case SC_EVENT_CONTROLLER_ERROR:
                LOGE("Controller error");
                return SCRCPY_EXIT_FAILURE;
            case SDL_EVENT_QUIT:
                LOGD("User requested to quit");
                return SCRCPY_EXIT_SUCCESS;
            case SC_EVENT_TIME_LIMIT_REACHED:
                LOGI("Time limit reached");
                return SCRCPY_EXIT_SUCCESS;
            default:
                sc_screen_handle_event(screen, &event);
                break;
        }
    }
    return SCRCPY_EXIT_FAILURE;
}

static void
mirror_timeout_on_timeout(struct sc_timeout *timeout, void *userdata) {
    (void) timeout;
    (void) userdata;
    sc_push_event(SC_EVENT_TIME_LIMIT_REACHED);
}

static void
set_terminal_title_with_prefix(const char *value) {
    char title[128];
    memcpy(title, "scrcpy - ", 9);
    size_t trunc_len = sc_str_utf8_truncation_index(value, 128 - 9 - 1);
    assert(trunc_len <= 128 - 9 - 1);
    memcpy(&title[9], value, trunc_len);
    title[9 + trunc_len] = '\0';
    sc_term_set_title(title);
}

enum scrcpy_exit_code
sc_mirror_run(const struct scrcpy_options *options) {
    static struct sc_mirror mirror_instance;
    struct sc_mirror *mirror = &mirror_instance;
    mirror->video_conn = SC_SOCKET_NONE;
    mirror->ctrl_conn = SC_SOCKET_NONE;
    mirror->video_pair_read = SC_SOCKET_NONE;
    mirror->video_pair_write = SC_SOCKET_NONE;
    mirror->ctrl_pair_read = SC_SOCKET_NONE;
    mirror->ctrl_pair_write = SC_SOCKET_NONE;

    static struct sc_demuxer demuxer;
    static struct sc_decoder decoder;
    static struct sc_video_regulator video_regulator;
    static struct sc_controller controller;
    static struct sc_keyboard_sdk keyboard_sdk;
    static struct sc_mouse_sdk mouse_sdk;
    static struct sc_screen screen;
    static struct sc_timeout timeout;

    enum scrcpy_exit_code ret = SCRCPY_EXIT_FAILURE;

    bool video_thread_started = false;
    bool ctrl_thread_started = false;
    bool controller_initialized = false;
    bool controller_started = false;
    bool screen_initialized = false;
    bool demuxer_started = false;
    bool timeout_initialized = false;
    bool timeout_started = false;

    char *device_name = NULL;
    char *state = NULL;
    char *video_source = NULL;
    bool flex_display = false;

    uint32_t addr;
    if (!mirror_resolve_host(options->daemon_host, &addr)) {
        return SCRCPY_EXIT_FAILURE;
    }
    uint16_t port = options->client_port;

    // Control/RPC connection first (its hello is reported to the user), then
    // the video connection, which becomes a one-way push stream
    mirror->ctrl_conn = mirror_connect(addr, port, &device_name, &state,
                                       &video_source, &flex_display);
    if (mirror->ctrl_conn == SC_SOCKET_NONE) {
        goto end;
    }
    LOGI("Connected to daemon on port %" PRIu16 " (device: %s, state: %s)",
         port, device_name && *device_name ? device_name : "?",
         state ? state : "?");

    bool camera = video_source && !strcmp(video_source, "camera");

    mirror->video_conn = mirror_connect(addr, port, NULL, NULL, NULL, NULL);
    if (mirror->video_conn == SC_SOCKET_NONE) {
        goto end;
    }
    static const char subscribe[] = "{\"id\":0,\"op\":\"subscribe_video\"}";
    if (!sc_daemon_write_frame(mirror->video_conn, subscribe,
                               sizeof(subscribe) - 1, NULL, 0)) {
        LOGE("Mirror: could not subscribe to the video stream");
        goto end;
    }

    // Apply the same SDL policy as the stock 4.1 path before SDL starts.
    sc_sdl_set_hints(options->render_driver, options->disable_screensaver);

    // Minimal SDL initialization (same sequence as scrcpy())
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        LOGE("Could not initialize SDL: %s", SDL_GetError());
        goto end;
    }
    atexit(SDL_Quit);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOGE("Could not initialize SDL video: %s", SDL_GetError());
        goto end;
    }

    if (!mirror_socketpair(&mirror->video_pair_read,
                           &mirror->video_pair_write)) {
        LOGE("Mirror: could not create local video stream pair");
        goto end;
    }

    static const struct sc_demuxer_callbacks demuxer_cbs = {
        .on_ended = mirror_demuxer_on_ended,
    };
    sc_demuxer_init(&demuxer, "video", mirror->video_pair_read, &demuxer_cbs,
                    NULL);
    sc_decoder_init(&decoder, "video");
    sc_packet_source_add_sink(&demuxer.packet_source, &decoder.packet_sink);

    struct sc_controller *controller_ptr = NULL;
    struct sc_key_processor *kp = NULL;
    struct sc_mouse_processor *mp = NULL;

    if (options->control) {
        if (!mirror_socketpair(&mirror->ctrl_pair_read,
                               &mirror->ctrl_pair_write)) {
            LOGE("Mirror: could not create local control pair");
            goto end;
        }

        static const struct sc_controller_callbacks controller_cbs = {
            .on_ended = mirror_controller_on_ended,
        };
        if (!sc_controller_init(&controller, mirror->ctrl_pair_write,
                                &controller_cbs, NULL)) {
            goto end;
        }
        controller_initialized = true;
        controller_ptr = &controller;

        if (!camera) {
            if (options->keyboard_input_mode
                    == SC_KEYBOARD_INPUT_MODE_SDK) {
                sc_keyboard_sdk_init(&keyboard_sdk, &controller,
                                     options->key_inject_mode,
                                     options->forward_key_repeat);
                kp = &keyboard_sdk.key_processor;
            }

            if (options->mouse_input_mode == SC_MOUSE_INPUT_MODE_SDK) {
                sc_mouse_sdk_init(&mouse_sdk, &controller,
                                  options->mouse_hover);
                mp = &mouse_sdk.mouse_processor;
            }
        }

        sc_controller_configure(&controller, NULL, NULL);
        if (!sc_controller_start(&controller)) {
            goto end;
        }
        controller_started = true;

        if (!sc_thread_create(&mirror->ctrl_thread, run_ctrl_adapter,
                              "scrcpy-mirror-c", mirror)) {
            LOGE("Mirror: could not start control adapter thread");
            goto end;
        }
        ctrl_thread_started = true;

        if (options->turn_screen_off) {
            struct sc_control_msg msg = {
                .type = SC_CONTROL_MSG_TYPE_SET_DISPLAY_POWER,
                .set_display_power.on = false,
            };
            if (!sc_controller_push_msg(&controller, &msg)) {
                LOGW("Could not request 'set display power'");
            }
        }
    }

    const char *window_title = options->window_title
                             ? options->window_title
                             : (device_name && *device_name ? device_name
                                                            : "scrcpy-auto");
    if (options->update_terminal_title) {
        set_terminal_title_with_prefix(window_title);
    }
    enum sc_render_fit render_fit = options->render_fit;
    if (render_fit == SC_RENDER_FIT_AUTO) {
        render_fit = flex_display ? SC_RENDER_FIT_UNSCALED
                                  : SC_RENDER_FIT_LETTERBOX;
    }

    struct sc_screen_params screen_params = {
        .video = true,
        .camera = camera,
        .flex_display = flex_display,
        .controller = controller_ptr,
        .fp = NULL,
        .kp = kp,
        .mp = mp,
        .gp = NULL,
        .mouse_bindings = options->mouse_bindings,
        .legacy_paste = options->legacy_paste,
        .clipboard_autosync = false, // needs the device session
        .shortcut_mods = options->shortcut_mods,
        .window_title = window_title,
        .always_on_top = options->always_on_top,
        .window_x = options->window_x,
        .window_y = options->window_y,
        .window_width = options->window_width,
        .window_height = options->window_height,
        .background_color = options->background_color,
        // A flex display is resized from the host window. Locking the host
        // window to the current device ratio would prevent arbitrary aspect
        // ratios and defeat the feature.
        .window_aspect_ratio_lock =
            flex_display ? false : options->window_aspect_ratio_lock,
        .window_borderless = options->window_borderless,
        .render_fit = render_fit,
        .orientation = options->display_orientation,
        .mipmaps = options->mipmaps,
        .fullscreen = options->fullscreen,
        .start_fps_counter = options->start_fps_counter,
    };

    if (!sc_screen_init(&screen, &screen_params)) {
        goto end;
    }
    screen_initialized = true;

    struct sc_frame_source *src = &decoder.frame_source;
    if (options->video_buffer) {
        sc_video_regulator_init(&video_regulator, options->video_buffer, true);
        sc_frame_source_add_sink(src, &video_regulator.frame_sink);
        src = &video_regulator.frame_source;
    }
    sc_frame_source_add_sink(src, &screen.frame_sink);

    if (!sc_thread_create(&mirror->video_thread, run_video_adapter,
                          "scrcpy-mirror-v", mirror)) {
        LOGE("Mirror: could not start video adapter thread");
        goto end;
    }
    video_thread_started = true;

    if (!sc_demuxer_start(&demuxer)) {
        goto end;
    }
    demuxer_started = true;

    if (options->time_limit) {
        if (!sc_timeout_init(&timeout)) {
            goto end;
        }
        timeout_initialized = true;
        static const struct sc_timeout_callbacks cbs = {
            .on_timeout = mirror_timeout_on_timeout,
        };
        if (!sc_timeout_start(&timeout,
                              sc_tick_now() + options->time_limit,
                              &cbs, NULL)) {
            goto end;
        }
        timeout_started = true;
    }

    ret = mirror_event_loop(&screen);

    sc_main_thread_stop();

end:
    if (timeout_started) {
        sc_timeout_stop(&timeout);
    }
    if (controller_started) {
        sc_controller_stop(&controller);
    }
    if (screen_initialized) {
        sc_screen_interrupt(&screen);
    }

    // Interrupt all sockets so every thread unblocks
    if (mirror->video_conn != SC_SOCKET_NONE) {
        net_interrupt(mirror->video_conn);
    }
    if (mirror->ctrl_conn != SC_SOCKET_NONE) {
        net_interrupt(mirror->ctrl_conn);
    }
    if (mirror->video_pair_read != SC_SOCKET_NONE) {
        net_interrupt(mirror->video_pair_read);
    }
    if (mirror->video_pair_write != SC_SOCKET_NONE) {
        net_interrupt(mirror->video_pair_write);
    }
    if (mirror->ctrl_pair_read != SC_SOCKET_NONE) {
        net_interrupt(mirror->ctrl_pair_read);
    }
    if (mirror->ctrl_pair_write != SC_SOCKET_NONE) {
        net_interrupt(mirror->ctrl_pair_write);
    }

    if (screen_initialized) {
        if (ret == SCRCPY_EXIT_DISCONNECTED) {
            sc_screen_handle_disconnection(&screen);
        }
        sc_screen_hide_window(&screen);
    }

    if (demuxer_started) {
        sc_demuxer_join(&demuxer);
    }
    if (video_thread_started) {
        sc_thread_join(&mirror->video_thread, NULL);
    }
    if (ctrl_thread_started) {
        sc_thread_join(&mirror->ctrl_thread, NULL);
    }

    // Destroy the screen only after the demuxer is joined (it may push frames)
    if (screen_initialized) {
        sc_screen_join(&screen);
        sc_screen_destroy(&screen);
    }

    if (controller_started) {
        sc_controller_join(&controller);
    }
    if (controller_initialized) {
        sc_controller_destroy(&controller);
    }
    if (timeout_started) {
        sc_timeout_join(&timeout);
    }
    if (timeout_initialized) {
        sc_timeout_destroy(&timeout);
    }

    if (mirror->video_pair_read != SC_SOCKET_NONE) {
        net_close(mirror->video_pair_read);
    }
    if (mirror->video_pair_write != SC_SOCKET_NONE) {
        net_close(mirror->video_pair_write);
    }
    if (mirror->ctrl_pair_read != SC_SOCKET_NONE) {
        net_close(mirror->ctrl_pair_read);
    }
    if (mirror->ctrl_pair_write != SC_SOCKET_NONE) {
        net_close(mirror->ctrl_pair_write);
    }
    if (mirror->video_conn != SC_SOCKET_NONE) {
        net_close(mirror->video_conn);
    }
    if (mirror->ctrl_conn != SC_SOCKET_NONE) {
        net_close(mirror->ctrl_conn);
    }

    free(device_name);
    free(state);
    free(video_source);
    return ret;
}
