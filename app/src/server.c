#include "server.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef _WIN32
# include <io.h>
# define isatty _isatty
# define fileno _fileno
#else
# include <unistd.h>
#endif

#include "adb/adb.h"
#include "util/env.h"
#include "util/file.h"
#include "util/log.h"
#include "util/net_intr.h"
#include "util/process.h"
#include "util/str.h"

#define SC_SERVER_FILENAME "scrcpy-server"

#define SC_SERVER_PATH_DEFAULT PREFIX "/share/scrcpy/" SC_SERVER_FILENAME
#define SC_DEVICE_SERVER_PATH "/data/local/tmp/scrcpy-server.jar"

#define SC_ADB_PORT_DEFAULT 5555
#define SC_SOCKET_NAME_PREFIX "scrcpy_"

static char *
get_server_path(void) {
    char *server_path = sc_get_env("SCRCPY_SERVER_PATH");
    if (server_path) {
        // if the envvar is set, use it
        LOGD("Using SCRCPY_SERVER_PATH: %s", server_path);
        return server_path;
    }

#ifndef PORTABLE
    LOGD("Using server: " SC_SERVER_PATH_DEFAULT);
    server_path = strdup(SC_SERVER_PATH_DEFAULT);
    if (!server_path) {
        LOG_OOM();
        return NULL;
    }
#else
    server_path = sc_file_get_local_path(SC_SERVER_FILENAME);
    if (!server_path) {
        LOGE("Could not get local file path, "
             "using " SC_SERVER_FILENAME " from current directory");
        return strdup(SC_SERVER_FILENAME);
    }

    LOGD("Using server (portable): %s", server_path);
#endif

    return server_path;
}

static bool
push_server(struct sc_intr *intr, const char *serial) {
    char *server_path = get_server_path();
    if (!server_path) {
        return false;
    }
    if (!sc_file_is_regular(server_path)) {
        LOGE("'%s' does not exist or is not a regular file\n", server_path);
        free(server_path);
        return false;
    }
    bool ok = sc_adb_push(intr, serial, server_path, SC_DEVICE_SERVER_PATH, 0);
    free(server_path);
    return ok;
}

static const char *
log_level_to_server_string(enum sc_log_level level) {
    switch (level) {
        case SC_LOG_LEVEL_VERBOSE:
            return "verbose";
        case SC_LOG_LEVEL_DEBUG:
            return "debug";
        case SC_LOG_LEVEL_INFO:
            return "info";
        case SC_LOG_LEVEL_WARN:
            return "warn";
        case SC_LOG_LEVEL_ERROR:
            return "error";
        default:
            assert(!"unexpected log level");
            return NULL;
    }
}

static bool
sc_server_sleep(struct sc_server *server, sc_tick deadline) {
    sc_mutex_lock(&server->mutex);
    bool timed_out = false;
    while (!server->stopped && !timed_out) {
        timed_out = !sc_cond_timedwait(&server->cond_stopped,
                                       &server->mutex, deadline);
    }
    bool stopped = server->stopped;
    sc_mutex_unlock(&server->mutex);

    return !stopped;
}

static const char *
sc_server_get_codec_name(enum sc_codec codec) {
    switch (codec) {
        case SC_CODEC_H264:
            return "h264";
        case SC_CODEC_H265:
            return "h265";
        case SC_CODEC_AV1:
            return "av1";
        case SC_CODEC_OPUS:
            return "opus";
        case SC_CODEC_AAC:
            return "aac";
        case SC_CODEC_FLAC:
            return "flac";
        case SC_CODEC_RAW:
            return "raw";
        default:
            assert(!"unexpected codec");
            return NULL;
    }
}

static const char *
sc_server_get_camera_facing_name(enum sc_camera_facing camera_facing) {
    switch (camera_facing) {
        case SC_CAMERA_FACING_FRONT:
            return "front";
        case SC_CAMERA_FACING_BACK:
            return "back";
        case SC_CAMERA_FACING_EXTERNAL:
            return "external";
        default:
            assert(!"unexpected camera facing");
            return NULL;
    }
}

static const char *
sc_server_get_audio_source_name(enum sc_audio_source audio_source) {
    switch (audio_source) {
        case SC_AUDIO_SOURCE_OUTPUT:
            return "output";
        case SC_AUDIO_SOURCE_MIC:
            return "mic";
        case SC_AUDIO_SOURCE_PLAYBACK:
            return "playback";
        case SC_AUDIO_SOURCE_MIC_UNPROCESSED:
            return "mic-unprocessed";
        case SC_AUDIO_SOURCE_MIC_CAMCORDER:
            return "mic-camcorder";
        case SC_AUDIO_SOURCE_MIC_VOICE_RECOGNITION:
            return "mic-voice-recognition";
        case SC_AUDIO_SOURCE_MIC_VOICE_COMMUNICATION:
            return "mic-voice-communication";
        case SC_AUDIO_SOURCE_VOICE_CALL:
            return "voice-call";
        case SC_AUDIO_SOURCE_VOICE_CALL_UPLINK:
            return "voice-call-uplink";
        case SC_AUDIO_SOURCE_VOICE_CALL_DOWNLINK:
            return "voice-call-downlink";
        case SC_AUDIO_SOURCE_VOICE_PERFORMANCE:
            return "voice-performance";
        default:
            assert(!"unexpected audio source");
            return NULL;
    }
}

static const char *
sc_server_get_display_ime_policy_name(enum sc_display_ime_policy policy) {
    switch (policy) {
        case SC_DISPLAY_IME_POLICY_LOCAL:
            return "local";
        case SC_DISPLAY_IME_POLICY_FALLBACK:
            return "fallback";
        case SC_DISPLAY_IME_POLICY_HIDE:
            return "hide";
        default:
            assert(!"unexpected display IME policy");
            return NULL;
    }
}

static bool
validate_string(const char *s) {
    // The parameters values are passed as command line arguments to adb, so
    // they must either be properly escaped, or they must not contain any
    // special shell characters.
    // Since they are not properly escaped on Windows anyway (see
    // sys/win/process.c), just forbid special shell characters.
    if (strpbrk(s, " ;'\"*$?&`#\\|<>[]{}()!~\r\n")) {
        LOGE("Invalid server param: [%s]", s);
        return false;
    }

    return true;
}

static sc_pid
execute_server(struct sc_server *server,
               const struct sc_server_params *params) {
    sc_pid pid = SC_PROCESS_NONE;

    const char *serial = server->serial;
    assert(serial);

    const char *cmd[128];
    unsigned count = 0;
    cmd[count++] = sc_adb_get_executable();
    cmd[count++] = "-s";
    cmd[count++] = serial;
    cmd[count++] = "shell";
    cmd[count++] = "CLASSPATH=" SC_DEVICE_SERVER_PATH;
    cmd[count++] = "app_process";

#ifdef SERVER_DEBUGGER
    uint16_t sdk_version = sc_adb_get_device_sdk_version(&server->intr, serial);
    if (!sdk_version) {
        LOGE("Could not determine SDK version");
        return 0;
    }

# define SERVER_DEBUGGER_PORT "5005"
    const char *dbg;
    if (sdk_version < 28) {
        // Android < 9
        dbg = "-agentlib:jdwp=transport=dt_socket,suspend=y,server=y,address="
              SERVER_DEBUGGER_PORT;
    } else if (sdk_version < 30) {
        // Android >= 9 && Android < 11
        dbg = "-XjdwpProvider:internal -XjdwpOptions:transport=dt_socket,"
              "suspend=y,server=y,address=" SERVER_DEBUGGER_PORT;
    } else {
        // Android >= 11
        // Contrary to the other methods, this does not suspend on start.
        // <https://github.com/Genymobile/scrcpy/pull/5466>
        dbg = "-XjdwpProvider:adbconnection";
    }
    cmd[count++] = dbg;
#endif

    cmd[count++] = "/"; // unused
    cmd[count++] = "com.genymobile.scrcpy.Server";
    cmd[count++] = SCRCPY_VERSION;

    unsigned dyn_idx = count; // from there, the strings are allocated
#define ADD_PARAM(fmt, ...) do { \
        char *p; \
        if (asprintf(&p, fmt, ## __VA_ARGS__) == -1) { \
            goto end; \
        } \
        cmd[count++] = p; \
    } while(0)
#define VALIDATE_STRING(s) do { \
        if (!validate_string(s)) { \
            goto end; \
        } \
    } while(0)

    ADD_PARAM("scid=%08x", params->scid);
    ADD_PARAM("log_level=%s", log_level_to_server_string(params->log_level));

    if (!params->video) {
        ADD_PARAM("video=false");
    }
    if (params->video_bit_rate) {
        ADD_PARAM("video_bit_rate=%" PRIu32, params->video_bit_rate);
    }
    if (!params->audio) {
        ADD_PARAM("audio=false");
    }
    if (params->audio_bit_rate) {
        ADD_PARAM("audio_bit_rate=%" PRIu32, params->audio_bit_rate);
    }
    if (params->video_codec != SC_CODEC_H264) {
        ADD_PARAM("video_codec=%s",
                  sc_server_get_codec_name(params->video_codec));
    }
    if (params->audio_codec != SC_CODEC_OPUS) {
        ADD_PARAM("audio_codec=%s",
            sc_server_get_codec_name(params->audio_codec));
    }
    if (params->video_source != SC_VIDEO_SOURCE_DISPLAY) {
        assert(params->video_source == SC_VIDEO_SOURCE_CAMERA);
        ADD_PARAM("video_source=camera");
    }
    // If audio is enabled, an "auto" audio source must have been resolved
    assert(params->audio_source != SC_AUDIO_SOURCE_AUTO || !params->audio);
    if (params->audio_source != SC_AUDIO_SOURCE_OUTPUT && params->audio) {
        ADD_PARAM("audio_source=%s",
                  sc_server_get_audio_source_name(params->audio_source));
    }
    if (params->audio_dup) {
        ADD_PARAM("audio_dup=true");
    }
    if (params->max_size) {
        ADD_PARAM("max_size=%" PRIu16, params->max_size);
    }
    if (params->max_fps) {
        VALIDATE_STRING(params->max_fps);
        ADD_PARAM("max_fps=%s", params->max_fps);
    }
    if (params->min_size_alignment != 1) {
        ADD_PARAM("min_size_alignment=%" PRIu8, params->min_size_alignment);
    }
    if (params->angle) {
        VALIDATE_STRING(params->angle);
        ADD_PARAM("angle=%s", params->angle);
    }
    if (params->capture_orientation_lock != SC_ORIENTATION_UNLOCKED
            || params->capture_orientation != SC_ORIENTATION_0) {
        if (params->capture_orientation_lock == SC_ORIENTATION_LOCKED_INITIAL) {
            ADD_PARAM("capture_orientation=@");
        } else {
            const char *orient =
                sc_orientation_get_name(params->capture_orientation);
            bool locked =
                params->capture_orientation_lock != SC_ORIENTATION_UNLOCKED;
            ADD_PARAM("capture_orientation=%s%s", locked ? "@" : "", orient);
        }
    }
    if (server->tunnel.forward) {
        ADD_PARAM("tunnel_forward=true");
    }
    if (params->crop) {
        VALIDATE_STRING(params->crop);
        ADD_PARAM("crop=%s", params->crop);
    }
    if (!params->control) {
        // By default, control is true
        ADD_PARAM("control=false");
    }
    if (params->display_id) {
        ADD_PARAM("display_id=%" PRIu32, params->display_id);
    }
    if (params->camera_id) {
        VALIDATE_STRING(params->camera_id);
        ADD_PARAM("camera_id=%s", params->camera_id);
    }
    if (params->camera_size) {
        VALIDATE_STRING(params->camera_size);
        ADD_PARAM("camera_size=%s", params->camera_size);
    }
    if (params->camera_facing != SC_CAMERA_FACING_ANY) {
        ADD_PARAM("camera_facing=%s",
            sc_server_get_camera_facing_name(params->camera_facing));
    }
    if (params->camera_ar) {
        VALIDATE_STRING(params->camera_ar);
        ADD_PARAM("camera_ar=%s", params->camera_ar);
    }
    if (params->camera_fps) {
        ADD_PARAM("camera_fps=%" PRIu16, params->camera_fps);
    }
    if (params->camera_high_speed) {
        ADD_PARAM("camera_high_speed=true");
    }
    if (params->camera_torch) {
        ADD_PARAM("camera_torch=true");
    }
    if (params->camera_zoom) {
        VALIDATE_STRING(params->camera_zoom);
        ADD_PARAM("camera_zoom=%s", params->camera_zoom);
    }
    if (params->show_touches) {
        ADD_PARAM("show_touches=true");
    }
    if (params->stay_awake) {
        ADD_PARAM("stay_awake=true");
    }
    if (params->screen_off_timeout != -1) {
        assert(params->screen_off_timeout >= 0);
        uint64_t ms = SC_TICK_TO_MS(params->screen_off_timeout);
        ADD_PARAM("screen_off_timeout=%" PRIu64, ms);
    }
    if (params->video_codec_options) {
        VALIDATE_STRING(params->video_codec_options);
        ADD_PARAM("video_codec_options=%s", params->video_codec_options);
    }
    if (params->audio_codec_options) {
        VALIDATE_STRING(params->audio_codec_options);
        ADD_PARAM("audio_codec_options=%s", params->audio_codec_options);
    }
    if (params->video_encoder) {
        VALIDATE_STRING(params->video_encoder);
        ADD_PARAM("video_encoder=%s", params->video_encoder);
    }
    if (params->audio_encoder) {
        VALIDATE_STRING(params->audio_encoder);
        ADD_PARAM("audio_encoder=%s", params->audio_encoder);
    }
    if (params->power_off_on_close) {
        ADD_PARAM("power_off_on_close=true");
    }
    if (!params->clipboard_autosync) {
        // By default, clipboard_autosync is true
        ADD_PARAM("clipboard_autosync=false");
    }
    if (!params->downsize_on_error) {
        // By default, downsize_on_error is true
        ADD_PARAM("downsize_on_error=false");
    }
    if (!params->cleanup) {
        // By default, cleanup is true
        ADD_PARAM("cleanup=false");
    }
    if (!params->power_on) {
        // By default, power_on is true
        ADD_PARAM("power_on=false");
    }
    if (params->new_display) {
        VALIDATE_STRING(params->new_display);
        ADD_PARAM("new_display=%s", params->new_display);
    }
    if (params->flex_display) {
        ADD_PARAM("flex_display=true");
    }
    if (params->display_ime_policy != SC_DISPLAY_IME_POLICY_UNDEFINED) {
        ADD_PARAM("display_ime_policy=%s",
            sc_server_get_display_ime_policy_name(params->display_ime_policy));
    }
    if (!params->vd_destroy_content) {
        ADD_PARAM("vd_destroy_content=false");
    }
    if (!params->vd_system_decorations) {
        ADD_PARAM("vd_system_decorations=false");
    }
    if (params->keep_active) {
        ADD_PARAM("keep_active=true");
    }
    if (params->list & SC_OPTION_LIST_ENCODERS) {
        ADD_PARAM("list_encoders=true");
    }
    if (params->list & SC_OPTION_LIST_DISPLAYS) {
        ADD_PARAM("list_displays=true");
    }
    if (params->list & SC_OPTION_LIST_CAMERAS) {
        ADD_PARAM("list_cameras=true");
    }
    if (params->list & SC_OPTION_LIST_CAMERA_SIZES) {
        ADD_PARAM("list_camera_sizes=true");
    }
    if (params->list & SC_OPTION_LIST_APPS) {
        ADD_PARAM("list_apps=true");
    }

#undef ADD_PARAM

    cmd[count++] = NULL;

#ifdef SERVER_DEBUGGER
    LOGI("Server debugger listening%s...",
         sdk_version < 30 ? " on port " SERVER_DEBUGGER_PORT : "");
    // For Android < 11, from the computer:
    //     - run `adb forward tcp:5005 tcp:5005`
    // For Android >= 11:
    //     - execute `adb jdwp` to get the jdwp port
    //     - run `adb forward tcp:5005 jdwp:XXXX` (replace XXXX)
    //
    // Then, from Android Studio: Run > Debug > Edit configurations...
    // On the left, click on '+', "Remote", with:
    //     Host: localhost
    //     Port: 5005
    // Then click on "Debug"
#endif
    // Inherit both stdout and stderr (all server logs are printed to stdout)
    pid = sc_adb_execute(cmd, 0);

end:
    for (unsigned i = dyn_idx; i < count; ++i) {
        free((char *) cmd[i]);
    }

    return pid;
}

static bool
connect_and_read_byte(struct sc_intr *intr, sc_socket socket,
                      uint32_t tunnel_host, uint16_t tunnel_port) {
    bool ok = net_connect_intr(intr, socket, tunnel_host, tunnel_port);
    if (!ok) {
        return false;
    }

    char byte;
    // the connection may succeed even if the server behind the "adb tunnel"
    // is not listening, so read one byte to detect a working connection
    if (net_recv_intr(intr, socket, &byte, 1) != 1) {
        // the server is not listening yet behind the adb tunnel
        return false;
    }

    return true;
}

static sc_socket
connect_to_server(struct sc_server *server, unsigned attempts, sc_tick delay,
                  uint32_t host, uint16_t port) {
    do {
        LOGD("Remaining connection attempts: %u", attempts);
        sc_socket socket = net_socket();
        if (socket != SC_SOCKET_NONE) {
            bool ok = connect_and_read_byte(&server->intr, socket, host, port);
            if (ok) {
                // it worked!
                return socket;
            }

            net_close(socket);
        }

        if (sc_intr_is_interrupted(&server->intr)) {
            // Stop immediately
            break;
        }

        if (attempts) {
            sc_tick deadline = sc_tick_now() + delay;
            bool ok = sc_server_sleep(server, deadline);
            if (!ok) {
                LOGI("Connection attempt stopped");
                break;
            }
        }
    } while (--attempts);
    return SC_SOCKET_NONE;
}

bool
sc_server_init(struct sc_server *server, const struct sc_server_params *params,
              const struct sc_server_callbacks *cbs, void *cbs_userdata) {
    // The allocated data in params (const char *) must remain valid until the
    // end of the program
    server->params = *params;

    bool ok = sc_adb_init();
    if (!ok) {
        return false;
    }

    ok = sc_mutex_init(&server->mutex);
    if (!ok) {
        sc_adb_destroy();
        return false;
    }

    ok = sc_cond_init(&server->cond_stopped);
    if (!ok) {
        sc_mutex_destroy(&server->mutex);
        sc_adb_destroy();
        return false;
    }

    ok = sc_intr_init(&server->intr);
    if (!ok) {
        sc_cond_destroy(&server->cond_stopped);
        sc_mutex_destroy(&server->mutex);
        sc_adb_destroy();
        return false;
    }

    server->serial = NULL;
    server->device_socket_name = NULL;
    server->stopped = false;

    server->video_socket = SC_SOCKET_NONE;
    server->audio_socket = SC_SOCKET_NONE;
    server->control_socket = SC_SOCKET_NONE;

    sc_adb_tunnel_init(&server->tunnel);

    assert(cbs);
    assert(cbs->on_connection_failed);
    assert(cbs->on_connected);
    assert(cbs->on_disconnected);

    server->cbs = cbs;
    server->cbs_userdata = cbs_userdata;

    return true;
}

static bool
device_read_info(struct sc_intr *intr, sc_socket device_socket,
                 struct sc_server_info *info) {
    uint8_t buf[SC_DEVICE_NAME_FIELD_LENGTH];
    ssize_t r = net_recv_all_intr(intr, device_socket, buf, sizeof(buf));
    if (r < SC_DEVICE_NAME_FIELD_LENGTH) {
        LOGE("Could not retrieve device information");
        return false;
    }
    // in case the client sends garbage
    buf[SC_DEVICE_NAME_FIELD_LENGTH - 1] = '\0';
    memcpy(info->device_name, (char *) buf, sizeof(info->device_name));

    return true;
}

static bool
sc_server_connect_to(struct sc_server *server, struct sc_server_info *info) {
    struct sc_adb_tunnel *tunnel = &server->tunnel;

    assert(tunnel->enabled);

    const char *serial = server->serial;
    assert(serial);

    bool video = server->params.video;
    bool audio = server->params.audio;
    bool control = server->params.control;

    sc_socket video_socket = SC_SOCKET_NONE;
    sc_socket audio_socket = SC_SOCKET_NONE;
    sc_socket control_socket = SC_SOCKET_NONE;
    if (!tunnel->forward) {
        if (video) {
            video_socket =
                net_accept_intr(&server->intr, tunnel->server_socket);
            if (video_socket == SC_SOCKET_NONE) {
                goto fail;
            }
        }

        if (audio) {
            audio_socket =
                net_accept_intr(&server->intr, tunnel->server_socket);
            if (audio_socket == SC_SOCKET_NONE) {
                goto fail;
            }
        }

        if (control) {
            control_socket =
                net_accept_intr(&server->intr, tunnel->server_socket);
            if (control_socket == SC_SOCKET_NONE) {
                goto fail;
            }
        }
    } else {
        uint32_t tunnel_host = server->params.tunnel_host;
        if (!tunnel_host) {
            tunnel_host = IPV4_LOCALHOST;
        }

        uint16_t tunnel_port = server->params.tunnel_port;
        if (!tunnel_port) {
            tunnel_port = tunnel->local_port;
        }

        unsigned attempts = 100;
        sc_tick delay = SC_TICK_FROM_MS(100);
        sc_socket first_socket = connect_to_server(server, attempts, delay,
                                                   tunnel_host, tunnel_port);
        if (first_socket == SC_SOCKET_NONE) {
            goto fail;
        }

        if (video) {
            video_socket = first_socket;
        }

        if (audio) {
            if (!video) {
                audio_socket = first_socket;
            } else {
                audio_socket = net_socket();
                if (audio_socket == SC_SOCKET_NONE) {
                    goto fail;
                }
                bool ok = net_connect_intr(&server->intr, audio_socket,
                                           tunnel_host, tunnel_port);
                if (!ok) {
                    goto fail;
                }
            }
        }

        if (control) {
            if (!video && !audio) {
                control_socket = first_socket;
            } else {
                control_socket = net_socket();
                if (control_socket == SC_SOCKET_NONE) {
                    goto fail;
                }
                bool ok = net_connect_intr(&server->intr, control_socket,
                                           tunnel_host, tunnel_port);
                if (!ok) {
                    goto fail;
                }
            }
        }
    }

    if (control_socket != SC_SOCKET_NONE) {
        // Disable Nagle's algorithm for the control socket
        // (it only impacts the sending side, so it is useless to set it
        // for the other sockets)
        bool ok = net_set_tcp_nodelay(control_socket, true);
        (void) ok; // error already logged
    }

    // we don't need the adb tunnel anymore
    sc_adb_tunnel_close(tunnel, &server->intr, serial,
                        server->device_socket_name);

    sc_socket first_socket = video ? video_socket
                           : audio ? audio_socket
                                   : control_socket;

    // The sockets will be closed on stop if device_read_info() fails
    bool ok = device_read_info(&server->intr, first_socket, info);
    if (!ok) {
        goto fail;
    }

    assert(!video || video_socket != SC_SOCKET_NONE);
    assert(!audio || audio_socket != SC_SOCKET_NONE);
    assert(!control || control_socket != SC_SOCKET_NONE);

    server->video_socket = video_socket;
    server->audio_socket = audio_socket;
    server->control_socket = control_socket;

    return true;

fail:
    if (video_socket != SC_SOCKET_NONE) {
        if (!net_close(video_socket)) {
            LOGW("Could not close video socket");
        }
    }

    if (audio_socket != SC_SOCKET_NONE) {
        if (!net_close(audio_socket)) {
            LOGW("Could not close audio socket");
        }
    }

    if (control_socket != SC_SOCKET_NONE) {
        if (!net_close(control_socket)) {
            LOGW("Could not close control socket");
        }
    }

    if (tunnel->enabled) {
        // Always leave this function with tunnel disabled
        sc_adb_tunnel_close(tunnel, &server->intr, serial,
                            server->device_socket_name);
    }

    return false;
}

static void
sc_server_on_terminated(void *userdata) {
    struct sc_server *server = userdata;

    // If the server process dies before connecting to the server socket,
    // then the client will be stuck forever on accept(). To avoid the problem,
    // wake up the accept() call (or any other) when the server dies, like on
    // stop() (it is safe to call interrupt() twice).
    sc_intr_interrupt(&server->intr);

    server->cbs->on_disconnected(server, server->cbs_userdata);

    LOGD("Server terminated");
}

static uint16_t
get_adb_tcp_port(struct sc_server *server, const char *serial) {
    struct sc_intr *intr = &server->intr;

    char *current_port =
        sc_adb_getprop(intr, serial, "service.adb.tcp.port", SC_ADB_SILENT);
    if (!current_port) {
        return 0;
    }

    long value;
    bool ok = sc_str_parse_integer(current_port, &value);
    free(current_port);
    if (!ok) {
        return 0;
    }

    if (value < 0 || value > 0xFFFF) {
        return 0;
    }

    return value;
}

static bool
wait_tcpip_mode_enabled(struct sc_server *server, const char *serial,
                        uint16_t expected_port, unsigned attempts,
                        sc_tick delay) {
    uint16_t adb_port = get_adb_tcp_port(server, serial);
    if (adb_port == expected_port) {
        return true;
    }

    // Only print this log if TCP/IP is not enabled
    LOGI("Waiting for TCP/IP mode enabled...");

    do {
        sc_tick deadline = sc_tick_now() + delay;
        if (!sc_server_sleep(server, deadline)) {
            LOGI("TCP/IP mode waiting interrupted");
            return false;
        }

        adb_port = get_adb_tcp_port(server, serial);
        if (adb_port == expected_port) {
            return true;
        }
    } while (--attempts);
    return false;
}

static char *
append_port(const char *ip, uint16_t port) {
    char *ip_port;
    int ret = asprintf(&ip_port, "%s:%" PRIu16, ip, port);
    if (ret == -1) {
        LOG_OOM();
        return NULL;
    }

    return ip_port;
}

static char *
sc_server_switch_to_tcpip(struct sc_server *server, const char *serial) {
    assert(serial);

    struct sc_intr *intr = &server->intr;

    LOGI("Switching device %s to TCP/IP...", serial);

    char *ip = sc_adb_get_device_ip(intr, serial, 0);
    if (!ip) {
        LOGE("Device IP not found");
        return NULL;
    }

    uint16_t adb_port = get_adb_tcp_port(server, serial);
    if (adb_port) {
        LOGI("TCP/IP mode already enabled on port %" PRIu16, adb_port);
    } else {
        LOGI("Enabling TCP/IP mode on port " SC_STR(SC_ADB_PORT_DEFAULT) "...");

        bool ok = sc_adb_tcpip(intr, serial, SC_ADB_PORT_DEFAULT,
                               SC_ADB_NO_STDOUT);
        if (!ok) {
            LOGE("Could not restart adbd in TCP/IP mode");
            free(ip);
            return NULL;
        }

        unsigned attempts = 40;
        sc_tick delay = SC_TICK_FROM_MS(250);
        ok = wait_tcpip_mode_enabled(server, serial, SC_ADB_PORT_DEFAULT,
                                     attempts, delay);
        if (!ok) {
            free(ip);
            return NULL;
        }

        adb_port = SC_ADB_PORT_DEFAULT;
        LOGI("TCP/IP mode enabled on port " SC_STR(SC_ADB_PORT_DEFAULT));
    }

    char *ip_port = append_port(ip, adb_port);
    free(ip);
    return ip_port;
}

static bool
sc_server_connect_to_tcpip(struct sc_server *server, const char *ip_port,
                           bool disconnect) {
    struct sc_intr *intr = &server->intr;

    if (disconnect) {
        // Error expected if not connected, do not report any error
        sc_adb_disconnect(intr, ip_port, SC_ADB_SILENT);
    }

    LOGI("Connecting to %s...", ip_port);

    bool ok = sc_adb_connect(intr, ip_port, 0);
    if (!ok) {
        LOGE("Could not connect to %s", ip_port);
        return false;
    }

    LOGI("Connected to %s", ip_port);
    return true;
}

static bool
sc_server_configure_tcpip_known_address(struct sc_server *server,
                                        const char *addr, bool disconnect) {
    // Append ":5555" if no port is present
    bool contains_port = strchr(addr, ':');
    char *ip_port = contains_port ? strdup(addr)
                                  : append_port(addr, SC_ADB_PORT_DEFAULT);
    if (!ip_port) {
        LOG_OOM();
        return false;
    }

    server->serial = ip_port;
    return sc_server_connect_to_tcpip(server, ip_port, disconnect);
}

static bool
sc_server_configure_tcpip_unknown_address(struct sc_server *server,
                                          const char *serial) {
    bool is_already_tcpip =
        sc_adb_device_get_type(serial) == SC_ADB_DEVICE_TYPE_TCPIP;
    if (is_already_tcpip) {
        // Nothing to do
        LOGI("Device already connected via TCP/IP: %s", serial);
        server->serial = strdup(serial);
        if (!server->serial) {
            LOG_OOM();
            return false;
        }
        return true;
    }

    char *ip_port = sc_server_switch_to_tcpip(server, serial);
    if (!ip_port) {
        return false;
    }

    server->serial = ip_port;
    return sc_server_connect_to_tcpip(server, ip_port, false);
}

static void
sc_server_kill_adb_if_requested(struct sc_server *server) {
    if (server->params.kill_adb_on_close) {
        LOGI("Killing adb server...");
        unsigned flags = SC_ADB_NO_STDOUT | SC_ADB_NO_STDERR | SC_ADB_NO_LOGERR;
        sc_adb_kill_server(&server->intr, flags);
    }
}

#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif

struct menu_item {
    char *display_name;
    char *serial;
    bool is_active;
    bool is_emulator;
};

struct sc_vec_menu_items SC_VECTOR(struct menu_item);

static bool
sc_adb_select_device_via_menu(struct sc_intr *intr, struct sc_server *server,
                              struct sc_adb_device *out_device) {
    struct sc_vec_adb_devices active_devices = SC_VECTOR_INITIALIZER;
    bool list_ok = sc_adb_list_devices(intr, SC_ADB_SILENT, &active_devices);
    if (!list_ok) {
        LOGE("Could not list ADB devices");
        return false;
    }

    struct sc_vec_str avds = SC_VECTOR_INITIALIZER;
    list_ok = sc_adb_list_avds(intr, SC_ADB_SILENT, &avds);
    if (!list_ok) {
        sc_adb_devices_destroy(&active_devices);
        return false;
    }

    struct sc_vec_menu_items items = SC_VECTOR_INITIALIZER;

    // First add active devices
    for (size_t i = 0; i < active_devices.size; ++i) {
        struct sc_adb_device *d = &active_devices.data[i];
        struct menu_item item;
        item.is_active = true;
        item.is_emulator = (sc_adb_device_get_type(d->serial) == SC_ADB_DEVICE_TYPE_EMULATOR);
        item.serial = strdup(d->serial);
        if (!item.serial) {
            LOG_OOM();
            goto error;
        }
        
        char *model_or_serial = d->model ? d->model : d->serial;
        char *status = d->state;
        char *display = NULL;
        if (asprintf(&display, "%s (%s, %s)", model_or_serial, d->serial, status) != -1) {
            item.display_name = display;
        } else {
            item.display_name = strdup(model_or_serial);
        }
        if (!item.display_name) {
            LOG_OOM();
            free(item.serial);
            goto error;
        }
        
        if (!sc_vector_push(&items, item)) {
            LOG_OOM();
            free(item.display_name);
            free(item.serial);
            goto error;
        }
    }

    // Now add AVDs that are not already running
    for (size_t i = 0; i < avds.size; ++i) {
        const char *avd_name = avds.data[i];
        bool is_running = false;
        
        for (size_t j = 0; j < active_devices.size; ++j) {
            struct sc_adb_device *d = &active_devices.data[j];
            if (sc_adb_device_get_type(d->serial) == SC_ADB_DEVICE_TYPE_EMULATOR) {
                char *running_avd = sc_adb_getprop(intr, d->serial, "ro.boot.qemu.avd_name", SC_ADB_SILENT);
                if (running_avd) {
                    if (strcmp(running_avd, avd_name) == 0) {
                        is_running = true;
                    }
                    free(running_avd);
                }
                if (is_running) {
                    break;
                }
            }
        }
        
        if (!is_running) {
            struct menu_item item;
            item.is_active = false;
            item.is_emulator = true;
            item.serial = strdup(avd_name);
            if (!item.serial) {
                LOG_OOM();
                goto error;
            }
            
            char *display = NULL;
            if (asprintf(&display, "%s (Offline Emulator)", avd_name) != -1) {
                item.display_name = display;
            } else {
                item.display_name = strdup(avd_name);
            }
            if (!item.display_name) {
                LOG_OOM();
                free(item.serial);
                goto error;
            }
            
            if (!sc_vector_push(&items, item)) {
                LOG_OOM();
                free(item.display_name);
                free(item.serial);
                goto error;
            }
        }
    }

    // Clean up temporary avds list
    for (size_t i = 0; i < avds.size; ++i) {
        free(avds.data[i]);
    }
    sc_vector_destroy(&avds);

    if (items.size == 0) {
        LOGE("No devices or emulators found.");
        sc_adb_devices_destroy(&active_devices);
        return false;
    }

    // If only 1 option and it is active, bypass the menu
    if (items.size == 1 && items.data[0].is_active) {
        size_t active_idx = 0;
        for (size_t i = 0; i < active_devices.size; ++i) {
            if (strcmp(active_devices.data[i].serial, items.data[0].serial) == 0) {
                active_idx = i;
                break;
            }
        }
        
        sc_adb_device_move(out_device, &active_devices.data[active_idx]);
        
        for (size_t i = 0; i < items.size; ++i) {
            free(items.data[i].display_name);
            free(items.data[i].serial);
        }
        sc_vector_destroy(&items);
        sc_adb_devices_destroy(&active_devices);
        return true;
    }

    // Display menu
    printf("\n========================================\n");
    printf("   scrcpy: Device Selection Menu\n");
    printf("========================================\n");
    for (size_t i = 0; i < items.size; ++i) {
        printf("[%zu] %s\n", i + 1, items.data[i].display_name);
    }
    printf("----------------------------------------\n");
    
    int choice = -1;
    while (choice < 1 || choice > (int)items.size) {
        printf("Select device [1-%zu] (q to quit): ", items.size);
        fflush(stdout);
        
        char choice_str[16];
        if (!fgets(choice_str, sizeof(choice_str), stdin)) {
            break;
        }
        
        size_t len = strlen(choice_str);
        if (len > 0 && choice_str[len-1] == '\n') {
            choice_str[len-1] = '\0';
        }
        
        if (strcmp(choice_str, "q") == 0 || strcmp(choice_str, "Q") == 0) {
            choice = -2;
            break;
        }
        
        char *endptr;
        long val = strtol(choice_str, &endptr, 10);
        if (endptr != choice_str && *endptr == '\0' && val >= 1 && val <= (long)items.size) {
            choice = (int)val;
        } else {
            printf("Invalid choice. Please try again.\n");
        }
    }

    if (choice < 1) {
        for (size_t i = 0; i < items.size; ++i) {
            free(items.data[i].display_name);
            free(items.data[i].serial);
        }
        sc_vector_destroy(&items);
        sc_adb_devices_destroy(&active_devices);
        return false;
    }

    struct menu_item selected_item = items.data[choice - 1];
    bool success = false;

    if (selected_item.is_active) {
        size_t active_idx = 0;
        for (size_t i = 0; i < active_devices.size; ++i) {
            if (strcmp(active_devices.data[i].serial, selected_item.serial) == 0) {
                active_idx = i;
                break;
            }
        }
        sc_adb_device_move(out_device, &active_devices.data[active_idx]);
        success = true;
    } else {
        printf("Starting emulator: %s...\n", selected_item.serial);
        
        char *cmd = NULL;
#ifdef _WIN32
        if (asprintf(&cmd, "start /b emulator -avd \"%s\"", selected_item.serial) != -1) {
            const char *const argv[] = { "cmd.exe", "/c", cmd, NULL };
            sc_pid pid;
            if (sc_process_execute(argv, &pid, SC_PROCESS_NO_STDOUT | SC_PROCESS_NO_STDERR) == SC_PROCESS_SUCCESS) {
                sc_process_close(pid);
            }
            free(cmd);
        }
#else
        if (asprintf(&cmd, "emulator -avd \"%s\" >/dev/null 2>&1 &", selected_item.serial) != -1) {
            const char *const argv[] = { "sh", "-c", cmd, NULL };
            sc_pid pid;
            if (sc_process_execute(argv, &pid, SC_PROCESS_NO_STDOUT | SC_PROCESS_NO_STDERR) == SC_PROCESS_SUCCESS) {
                sc_process_close(pid);
            }
            free(cmd);
        }
#endif

        printf("Waiting for emulator to connect (timeout 60s)...");
        fflush(stdout);
        
        bool found = false;
        char *found_serial = NULL;
        
        for (int attempt = 0; attempt < 60; ++attempt) {
            sc_tick deadline = sc_tick_now() + SC_TICK_FROM_MS(1000);
            sc_server_sleep(server, deadline);
            
            if (sc_intr_is_interrupted(intr)) {
                break;
            }
            
            struct sc_vec_adb_devices adb_devices = SC_VECTOR_INITIALIZER;
            if (sc_adb_list_devices(intr, SC_ADB_SILENT, &adb_devices)) {
                for (size_t i = 0; i < adb_devices.size; ++i) {
                    struct sc_adb_device *d = &adb_devices.data[i];
                    if (sc_adb_device_get_type(d->serial) == SC_ADB_DEVICE_TYPE_EMULATOR) {
                        char *cur_avd = sc_adb_getprop(intr, d->serial, "ro.boot.qemu.avd_name", SC_ADB_SILENT);
                        if (cur_avd) {
                            bool match = (strcmp(cur_avd, selected_item.serial) == 0);
                            free(cur_avd);
                            if (match && strcmp(d->state, "device") == 0) {
                                char *boot_completed = sc_adb_getprop(intr, d->serial, "sys.boot_completed", SC_ADB_SILENT);
                                if (boot_completed) {
                                    bool is_booted = (strcmp(boot_completed, "1") == 0);
                                    free(boot_completed);
                                    if (is_booted) {
                                        found_serial = strdup(d->serial);
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                sc_adb_devices_destroy(&adb_devices);
            }
            
            if (found) {
                break;
            }
            printf(".");
            fflush(stdout);
        }
        
        if (found) {
            printf("\nEmulator connected!\n");
            out_device->serial = found_serial;
            out_device->state = strdup("device");
            out_device->model = strdup(selected_item.serial);
            out_device->selected = true;
            success = true;
        } else {
            printf("\nFailed to connect to emulator within 60s.\n");
        }
    }

    for (size_t i = 0; i < items.size; ++i) {
        free(items.data[i].display_name);
        free(items.data[i].serial);
    }
    sc_vector_destroy(&items);
    sc_adb_devices_destroy(&active_devices);

    return success;

error:
    for (size_t i = 0; i < items.size; ++i) {
        free(items.data[i].display_name);
        free(items.data[i].serial);
    }
    sc_vector_destroy(&items);
    for (size_t i = 0; i < avds.size; ++i) {
        free(avds.data[i]);
    }
    sc_vector_destroy(&avds);
    sc_adb_devices_destroy(&active_devices);
    return false;
}

static int
run_server(void *data) {
    struct sc_server *server = data;

    const struct sc_server_params *params = &server->params;

    // Execute "adb start-server" before "adb devices" so that daemon starting
    // output/errors is correctly printed in the console ("adb devices" output
    // is parsed, so it is not output)
    bool ok = sc_adb_start_server(&server->intr, 0);
    if (!ok) {
        LOGE("Could not start adb server");
        goto error_connection_failed;
    }

    // params->tcpip_dst implies params->tcpip
    assert(!params->tcpip_dst || params->tcpip);

    // If tcpip_dst parameter is given, then it must connect to this address.
    // Therefore, the device is unknown, so serial is meaningless at this point.
    assert(!params->req_serial || !params->tcpip_dst);

    // A device must be selected via a serial in all cases except when --tcpip=
    // is called with a parameter (in that case, the device may initially not
    // exist, and scrcpy will execute "adb connect").
    bool need_initial_serial = !params->tcpip_dst;

    if (need_initial_serial) {
        // At most one of the 3 following parameters may be set
        assert(!!params->req_serial
               + params->select_usb
               + params->select_tcpip <= 1);

        struct sc_adb_device_selector selector;
        if (params->req_serial) {
            selector.type = SC_ADB_DEVICE_SELECT_SERIAL;
            selector.serial = params->req_serial;
        } else if (params->select_usb) {
            selector.type = SC_ADB_DEVICE_SELECT_USB;
        } else if (params->select_tcpip) {
            selector.type = SC_ADB_DEVICE_SELECT_TCPIP;
        } else {
            // No explicit selection, check $ANDROID_SERIAL
            const char *env_serial = getenv("ANDROID_SERIAL");
            if (env_serial) {
                LOGI("Using ANDROID_SERIAL: %s", env_serial);
                selector.type = SC_ADB_DEVICE_SELECT_SERIAL;
                selector.serial = env_serial;
            } else {
                selector.type = SC_ADB_DEVICE_SELECT_ALL;
            }
        }

        bool use_menu = false;
        if (!params->req_serial && !params->select_usb && !params->select_tcpip && !getenv("ANDROID_SERIAL")) {
#ifdef _WIN32
            if (isatty(0) && isatty(1)) {
                use_menu = true;
            }
#else
            if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
                use_menu = true;
            }
#endif
        }

        struct sc_adb_device device;
        if (use_menu) {
            ok = sc_adb_select_device_via_menu(&server->intr, server, &device);
        } else {
            ok = sc_adb_select_device(&server->intr, &selector, 0, &device);
        }
        if (!ok) {
            goto error_connection_failed;
        }

        if (params->tcpip) {
            assert(!params->tcpip_dst);
            ok = sc_server_configure_tcpip_unknown_address(server,
                                                           device.serial);
            sc_adb_device_destroy(&device);
            if (!ok) {
                goto error_connection_failed;
            }
            assert(server->serial);
        } else {
            // "move" the device.serial without copy
            server->serial = device.serial;
            // the serial must not be freed by the destructor
            device.serial = NULL;
            sc_adb_device_destroy(&device);
        }
    } else {
        // If the user passed a '+' (--tcpip=+ip), then disconnect first
        const char *tcpip_dst = params->tcpip_dst;
        bool plus = tcpip_dst[0] == '+';
        if (plus) {
            ++tcpip_dst;
        }
        ok = sc_server_configure_tcpip_known_address(server, tcpip_dst, plus);
        if (!ok) {
            goto error_connection_failed;
        }
    }

    const char *serial = server->serial;
    assert(serial);
    LOGD("Device serial: %s", serial);

    ok = push_server(&server->intr, serial);
    if (!ok) {
        goto error_connection_failed;
    }

    // If --list-* is passed, then the server just prints the requested data
    // then exits.
    if (params->list) {
        sc_pid pid = execute_server(server, params);
        if (pid == SC_PROCESS_NONE) {
            goto error_connection_failed;
        }
        sc_process_wait(pid, NULL); // ignore exit code
        sc_process_close(pid);
        // Wake up await_for_server()
        server->cbs->on_connected(server, server->cbs_userdata);
        return 0;
    }

    int r = asprintf(&server->device_socket_name, SC_SOCKET_NAME_PREFIX "%08x",
                     params->scid);
    if (r == -1) {
        LOG_OOM();
        goto error_connection_failed;
    }
    assert(r == sizeof(SC_SOCKET_NAME_PREFIX) - 1 + 8);
    assert(server->device_socket_name);

    ok = sc_adb_tunnel_open(&server->tunnel, &server->intr, serial,
                            server->device_socket_name, params->port_range,
                            params->force_adb_forward);
    if (!ok) {
        goto error_connection_failed;
    }

    // server will connect to our server socket
    sc_pid pid = execute_server(server, params);
    if (pid == SC_PROCESS_NONE) {
        sc_adb_tunnel_close(&server->tunnel, &server->intr, serial,
                            server->device_socket_name);
        goto error_connection_failed;
    }

    static const struct sc_process_listener listener = {
        .on_terminated = sc_server_on_terminated,
    };
    struct sc_process_observer observer;
    ok = sc_process_observer_init(&observer, pid, &listener, server);
    if (!ok) {
        sc_process_terminate(pid);
        sc_process_wait(pid, true); // ignore exit code
        sc_adb_tunnel_close(&server->tunnel, &server->intr, serial,
                            server->device_socket_name);
        goto error_connection_failed;
    }

    ok = sc_server_connect_to(server, &server->info);
    // The tunnel is always closed by server_connect_to()
    if (!ok) {
        sc_process_terminate(pid);
        sc_process_wait(pid, true); // ignore exit code
        sc_process_observer_join(&observer);
        sc_process_observer_destroy(&observer);
        goto error_connection_failed;
    }

    // Now connected
    server->cbs->on_connected(server, server->cbs_userdata);

    // Wait for server_stop()
    sc_mutex_lock(&server->mutex);
    while (!server->stopped) {
        sc_cond_wait(&server->cond_stopped, &server->mutex);
    }
    sc_mutex_unlock(&server->mutex);

    // Interrupt sockets to wake up socket blocking calls on the server

    if (server->video_socket != SC_SOCKET_NONE) {
        // There is no video_socket if --no-video is set
        net_interrupt(server->video_socket);
    }

    if (server->audio_socket != SC_SOCKET_NONE) {
        // There is no audio_socket if --no-audio is set
        net_interrupt(server->audio_socket);
    }

    if (server->control_socket != SC_SOCKET_NONE) {
        // There is no control_socket if --no-control is set
        net_interrupt(server->control_socket);
    }

    // Give some delay for the server to terminate properly
#define WATCHDOG_DELAY SC_TICK_FROM_SEC(1)
    sc_tick deadline = sc_tick_now() + WATCHDOG_DELAY;
    bool terminated = sc_process_observer_timedwait(&observer, deadline);

    // After this delay, kill the server if it's not dead already.
    // On some devices, closing the sockets is not sufficient to wake up the
    // blocking calls while the device is asleep.
    if (!terminated) {
        // The process may have terminated since the check, but it is not
        // reaped (closed) yet, so its PID is still valid, and it is ok to call
        // sc_process_terminate() even in that case.
        LOGW("Killing the server...");
        sc_process_terminate(pid);
    }

    sc_process_observer_join(&observer);
    sc_process_observer_destroy(&observer);

    sc_process_close(pid);

    sc_server_kill_adb_if_requested(server);

    return 0;

error_connection_failed:
    sc_server_kill_adb_if_requested(server);
    server->cbs->on_connection_failed(server, server->cbs_userdata);
    return -1;
}

bool
sc_server_start(struct sc_server *server) {
    bool ok =
        sc_thread_create(&server->thread, run_server, "scrcpy-server", server);
    if (!ok) {
        LOGE("Could not create server thread");
        return false;
    }

    return true;
}

void
sc_server_stop(struct sc_server *server) {
    sc_mutex_lock(&server->mutex);
    server->stopped = true;
    sc_cond_signal(&server->cond_stopped);
    sc_intr_interrupt(&server->intr);
    sc_mutex_unlock(&server->mutex);
}

void
sc_server_join(struct sc_server *server) {
    sc_thread_join(&server->thread, NULL);
}

void
sc_server_destroy(struct sc_server *server) {
    if (server->video_socket != SC_SOCKET_NONE) {
        net_close(server->video_socket);
    }
    if (server->audio_socket != SC_SOCKET_NONE) {
        net_close(server->audio_socket);
    }
    if (server->control_socket != SC_SOCKET_NONE) {
        net_close(server->control_socket);
    }

    free(server->serial);
    free(server->device_socket_name);
    sc_intr_destroy(&server->intr);
    sc_cond_destroy(&server->cond_stopped);
    sc_mutex_destroy(&server->mutex);

    sc_adb_destroy();
}
