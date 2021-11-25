#include "server.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_platform.h>

#include "adb.h"
#include "util/file.h"
#include "util/log.h"
#include "util/net_intr.h"
#include "util/process_intr.h"
#include "util/str.h"

#define SC_SERVER_FILENAME "scrcpy-server"

#define SC_SERVER_PATH_DEFAULT PREFIX "/share/scrcpy/" SC_SERVER_FILENAME
#define SC_DEVICE_SERVER_PATH "/data/local/tmp/scrcpy-server.jar"

static char *
get_server_path(void) {
#ifdef __WINDOWS__
    const wchar_t *server_path_env = _wgetenv(L"SCRCPY_SERVER_PATH");
#else
    const char *server_path_env = getenv("SCRCPY_SERVER_PATH");
#endif
    if (server_path_env) {
        // if the envvar is set, use it
#ifdef __WINDOWS__
        char *server_path = sc_str_from_wchars(server_path_env);
#else
        char *server_path = strdup(server_path_env);
#endif
        if (!server_path) {
            LOG_OOM();
            return NULL;
        }
        LOGD("Using SCRCPY_SERVER_PATH: %s", server_path);
        return server_path;
    }

#ifndef PORTABLE
    LOGD("Using server: " SC_SERVER_PATH_DEFAULT);
    char *server_path = strdup(SC_SERVER_PATH_DEFAULT);
    if (!server_path) {
        LOG_OOM();
        return NULL;
    }
#else
    char *server_path = sc_file_get_local_path(SC_SERVER_FILENAME);
    if (!server_path) {
        LOGE("Could not get local file path, "
             "using " SC_SERVER_FILENAME " from current directory");
        return strdup(SC_SERVER_FILENAME);
    }

    LOGD("Using server (portable): %s", server_path);
#endif

    return server_path;
}

static void
sc_server_params_destroy(struct sc_server_params *params) {
    // The server stores a copy of the params provided by the user
    free((char *) params->serial);
    free((char *) params->crop);
    free((char *) params->codec_options);
    free((char *) params->encoder_name);
    free((char *) params->tcpip_dst);
}

static bool
sc_server_params_copy(struct sc_server_params *dst,
                      const struct sc_server_params *src) {
    *dst = *src;

    // The params reference user-allocated memory, so we must copy them to
    // handle them from another thread

#define COPY(FIELD) \
    dst->FIELD = NULL; \
    if (src->FIELD) { \
        dst->FIELD = strdup(src->FIELD); \
        if (!dst->FIELD) { \
            goto error; \
        } \
    }

    COPY(serial);
    COPY(crop);
    COPY(codec_options);
    COPY(encoder_name);
    COPY(tcpip_dst);
#undef COPY

    return true;

error:
    sc_server_params_destroy(dst);
    return false;
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
    bool ok = adb_push(intr, serial, server_path, SC_DEVICE_SERVER_PATH, 0);
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
            return "(unknown)";
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

static sc_pid
execute_server(struct sc_server *server,
               const struct sc_server_params *params) {
    sc_pid pid = SC_PROCESS_NONE;

    const char *cmd[128];
    unsigned count = 0;
    cmd[count++] = "shell";
    cmd[count++] = "CLASSPATH=" SC_DEVICE_SERVER_PATH;
    cmd[count++] = "app_process";

#ifdef SERVER_DEBUGGER
# define SERVER_DEBUGGER_PORT "5005"
    cmd[count++] =
# ifdef SERVER_DEBUGGER_METHOD_NEW
        /* Android 9 and above */
        "-XjdwpProvider:internal -XjdwpOptions:transport=dt_socket,suspend=y,"
        "server=y,address="
# else
        /* Android 8 and below */
        "-agentlib:jdwp=transport=dt_socket,suspend=y,server=y,address="
# endif
            SERVER_DEBUGGER_PORT;
#endif
    cmd[count++] = "/"; // unused
    cmd[count++] = "com.genymobile.scrcpy.Server";
    cmd[count++] = SCRCPY_VERSION;

    unsigned dyn_idx = count; // from there, the strings are allocated
#define ADD_PARAM(fmt, ...) { \
        char *p = (char *) &cmd[count]; \
        if (asprintf(&p, fmt, ## __VA_ARGS__) == -1) { \
            goto end; \
        } \
        cmd[count++] = p; \
    }
#define STRBOOL(v) (v ? "true" : "false")

    ADD_PARAM("log_level=%s", log_level_to_server_string(params->log_level));
    ADD_PARAM("bit_rate=%" PRIu32, params->bit_rate);

    if (params->max_size) {
        ADD_PARAM("max_size=%" PRIu16, params->max_size);
    }
    if (params->max_fps) {
        ADD_PARAM("max_fps=%" PRIu16, params->max_fps);
    }
    if (params->lock_video_orientation != SC_LOCK_VIDEO_ORIENTATION_UNLOCKED) {
        ADD_PARAM("lock_video_orientation=%" PRIi8,
                  params->lock_video_orientation);
    }
    if (server->tunnel.forward) {
        ADD_PARAM("tunnel_forward=%s", STRBOOL(server->tunnel.forward));
    }
    if (params->crop) {
        ADD_PARAM("crop=%s", params->crop);
    }
    if (!params->control) {
        // By default, control is true
        ADD_PARAM("control=%s", STRBOOL(params->control));
    }
    if (params->display_id) {
        ADD_PARAM("display_id=%" PRIu32, params->display_id);
    }
    if (params->show_touches) {
        ADD_PARAM("show_touches=%s", STRBOOL(params->show_touches));
    }
    if (params->stay_awake) {
        ADD_PARAM("stay_awake=%s", STRBOOL(params->stay_awake));
    }
    if (params->codec_options) {
        ADD_PARAM("codec_options=%s", params->codec_options);
    }
    if (params->encoder_name) {
        ADD_PARAM("encoder_name=%s", params->encoder_name);
    }
    if (params->power_off_on_close) {
        ADD_PARAM("power_off_on_close=%s", STRBOOL(params->power_off_on_close));
    }
    if (!params->clipboard_autosync) {
        // By defaut, clipboard_autosync is true
        ADD_PARAM("clipboard_autosync=%s", STRBOOL(params->clipboard_autosync));
    }

#undef ADD_PARAM
#undef STRBOOL

#ifdef SERVER_DEBUGGER
    LOGI("Server debugger waiting for a client on device port "
         SERVER_DEBUGGER_PORT "...");
    // From the computer, run
    //     adb forward tcp:5005 tcp:5005
    // Then, from Android Studio: Run > Debug > Edit configurations...
    // On the left, click on '+', "Remote", with:
    //     Host: localhost
    //     Port: 5005
    // Then click on "Debug"
#endif
    // Inherit both stdout and stderr (all server logs are printed to stdout)
    pid = adb_execute(params->serial, cmd, count, 0);

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
    bool ok = sc_server_params_copy(&server->params, params);
    if (!ok) {
        LOG_OOM();
        return false;
    }

    ok = sc_mutex_init(&server->mutex);
    if (!ok) {
        sc_server_params_destroy(&server->params);
        return false;
    }

    ok = sc_cond_init(&server->cond_stopped);
    if (!ok) {
        sc_mutex_destroy(&server->mutex);
        sc_server_params_destroy(&server->params);
        return false;
    }

    ok = sc_intr_init(&server->intr);
    if (!ok) {
        sc_cond_destroy(&server->cond_stopped);
        sc_mutex_destroy(&server->mutex);
        sc_server_params_destroy(&server->params);
        return false;
    }

    server->stopped = false;

    server->video_socket = SC_SOCKET_NONE;
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
    unsigned char buf[SC_DEVICE_NAME_FIELD_LENGTH + 4];
    ssize_t r = net_recv_all_intr(intr, device_socket, buf, sizeof(buf));
    if (r < SC_DEVICE_NAME_FIELD_LENGTH + 4) {
        LOGE("Could not retrieve device information");
        return false;
    }
    // in case the client sends garbage
    buf[SC_DEVICE_NAME_FIELD_LENGTH - 1] = '\0';
    memcpy(info->device_name, (char *) buf, sizeof(info->device_name));

    info->frame_size.width = (buf[SC_DEVICE_NAME_FIELD_LENGTH] << 8)
                           | buf[SC_DEVICE_NAME_FIELD_LENGTH + 1];
    info->frame_size.height = (buf[SC_DEVICE_NAME_FIELD_LENGTH + 2] << 8)
                            | buf[SC_DEVICE_NAME_FIELD_LENGTH + 3];
    return true;
}

static bool
sc_server_connect_to(struct sc_server *server, struct sc_server_info *info) {
    struct sc_adb_tunnel *tunnel = &server->tunnel;

    assert(tunnel->enabled);

    const char *serial = server->params.serial;

    sc_socket video_socket = SC_SOCKET_NONE;
    sc_socket control_socket = SC_SOCKET_NONE;
    if (!tunnel->forward) {
        video_socket = net_accept_intr(&server->intr, tunnel->server_socket);
        if (video_socket == SC_SOCKET_NONE) {
            goto fail;
        }

        control_socket = net_accept_intr(&server->intr, tunnel->server_socket);
        if (control_socket == SC_SOCKET_NONE) {
            goto fail;
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
        video_socket = connect_to_server(server, attempts, delay, tunnel_host,
                                         tunnel_port);
        if (video_socket == SC_SOCKET_NONE) {
            goto fail;
        }

        // we know that the device is listening, we don't need several attempts
        control_socket = net_socket();
        if (control_socket == SC_SOCKET_NONE) {
            goto fail;
        }
        bool ok = net_connect_intr(&server->intr, control_socket, tunnel_host,
                                   tunnel_port);
        if (!ok) {
            goto fail;
        }
    }

    // we don't need the adb tunnel anymore
    sc_adb_tunnel_close(tunnel, &server->intr, serial);

    // The sockets will be closed on stop if device_read_info() fails
    bool ok = device_read_info(&server->intr, video_socket, info);
    if (!ok) {
        goto fail;
    }

    assert(video_socket != SC_SOCKET_NONE);
    assert(control_socket != SC_SOCKET_NONE);

    server->video_socket = video_socket;
    server->control_socket = control_socket;

    return true;

fail:
    if (video_socket != SC_SOCKET_NONE) {
        if (!net_close(video_socket)) {
            LOGW("Could not close video socket");
        }
    }

    if (control_socket != SC_SOCKET_NONE) {
        if (!net_close(control_socket)) {
            LOGW("Could not close control socket");
        }
    }

    // Always leave this function with tunnel disabled
    sc_adb_tunnel_close(tunnel, &server->intr, serial);

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

static bool
sc_server_fill_serial(struct sc_server *server) {
    // Retrieve the actual device immediately if not provided, so that all
    // future adb commands are executed for this specific device, even if other
    // devices are connected afterwards (without "more than one
    // device/emulator" error)
    if (!server->params.serial) {
        // The serial is owned by sc_server_params, and will be freed on destroy
        server->params.serial = adb_get_serialno(&server->intr, 0);
        if (!server->params.serial) {
            LOGE("Could not get device serial");
            return false;
        }

        LOGD("Device serial: %s", server->params.serial);
    }

    return true;
}

static bool
is_tcpip_mode_enabled(struct sc_server *server) {
    struct sc_intr *intr = &server->intr;
    const char *serial = server->params.serial;

    char *current_port =
        adb_getprop(intr, serial, "service.adb.tcp.port", SC_ADB_SILENT);
    if (!current_port) {
        return false;
    }

    // Is the device is listening on TCP on port 5555?
    bool enabled = !strcmp("5555", current_port);
    free(current_port);
    return enabled;
}

static bool
wait_tcpip_mode_enabled(struct sc_server *server, unsigned attempts,
                        sc_tick delay) {
    if (is_tcpip_mode_enabled(server)) {
        LOGI("TCP/IP mode enabled");
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

        if (is_tcpip_mode_enabled(server)) {
            LOGI("TCP/IP mode enabled");
            return true;
        }
    } while (--attempts);
    return false;
}

char *
append_port_5555(const char *ip) {
    size_t len = strlen(ip);

    // sizeof counts the final '\0'
    char *ip_port = malloc(len + sizeof(":5555"));
    if (!ip_port) {
        LOG_OOM();
        return NULL;
    }

    memcpy(ip_port, ip, len);
    memcpy(ip_port + len, ":5555", sizeof(":5555"));

    return ip_port;
}

static bool
sc_server_switch_to_tcpip(struct sc_server *server, char **out_ip_port) {
    const char *serial = server->params.serial;
    assert(serial);

    struct sc_intr *intr = &server->intr;

    char *ip = adb_get_device_ip(intr, serial, 0);
    if (!ip) {
        LOGE("Device IP not found");
        return false;
    }

    char *ip_port = append_port_5555(ip);
    free(ip);
    if (!ip_port) {
        return false;
    }

    bool tcp_mode = is_tcpip_mode_enabled(server);

    if (!tcp_mode) {
        bool ok = adb_tcpip(intr, serial, 5555, SC_ADB_NO_STDOUT);
        if (!ok) {
            LOGE("Could not restart adbd in TCP/IP mode");
            goto error;
        }

        unsigned attempts = 40;
        sc_tick delay = SC_TICK_FROM_MS(250);
        ok = wait_tcpip_mode_enabled(server, attempts, delay);
        if (!ok) {
            goto error;
        }
    }

    *out_ip_port = ip_port;

    return true;

error:
    free(ip_port);
    return false;
}

static bool
sc_server_connect_to_tcpip(struct sc_server *server, const char *ip_port) {
    struct sc_intr *intr = &server->intr;

    // Error expected if not connected, do not report any error
    adb_disconnect(intr, ip_port, SC_ADB_SILENT);

    bool ok = adb_connect(intr, ip_port, 0);
    if (!ok) {
        LOGE("Could not connect to %s", ip_port);
        return false;
    }

    // Override the serial, owned by the sc_server_params
    free((void *) server->params.serial);
    server->params.serial = strdup(ip_port);
    if (!server->params.serial) {
        LOG_OOM();
        return false;
    }

    LOGI("Connected to %s", ip_port);
    return true;
}


static bool
sc_server_configure_tcpip(struct sc_server *server) {
    char *ip_port;

    const struct sc_server_params *params = &server->params;

    // If tcpip parameter is given, then it must connect to this address.
    // Therefore, the device is unknown, so serial is meaningless at this point.
    assert(!params->serial || !params->tcpip_dst);

    if (params->tcpip_dst) {
        // Append ":5555" if no port is present
        bool contains_port = strchr(params->tcpip_dst, ':');
        ip_port = contains_port ? strdup(params->tcpip_dst)
                                : append_port_5555(params->tcpip_dst);
        if (!ip_port) {
            LOG_OOM();
            return false;
        }
    } else {
        // The device IP address must be retrieved from the current
        // connected device
        if (!sc_server_fill_serial(server)) {
            return false;
        }

        // The serial is either the real serial when connected via USB, or
        // the IP:PORT when connected over TCP/IP. Only the latter contains
        // a colon.
        bool is_already_tcpip = strchr(params->serial, ':');
        if (is_already_tcpip) {
            // Nothing to do
            LOGI("Device already connected via TCP/IP: %s", params->serial);
            return true;
        }

        bool ok = sc_server_switch_to_tcpip(server, &ip_port);
        if (!ok) {
            return false;
        }
    }

    // On success, this call changes params->serial
    bool ok = sc_server_connect_to_tcpip(server, ip_port);
    free(ip_port);
    return ok;
}

static int
run_server(void *data) {
    struct sc_server *server = data;

    const struct sc_server_params *params = &server->params;

    if (params->serial) {
        LOGD("Device serial: %s", params->serial);
    }

    if (params->tcpip) {
        // params->serial may be changed after this call
        bool ok = sc_server_configure_tcpip(server);
        if (!ok) {
            goto error_connection_failed;
        }
    }

    // It is ok to call this function even if the device serial has been
    // changed by switching over TCP/IP
    if (!sc_server_fill_serial(server)) {
        goto error_connection_failed;
    }

    bool ok = push_server(&server->intr, params->serial);
    if (!ok) {
        goto error_connection_failed;
    }

    ok = sc_adb_tunnel_open(&server->tunnel, &server->intr, params->serial,
                            params->port_range, params->force_adb_forward);
    if (!ok) {
        goto error_connection_failed;
    }

    // server will connect to our server socket
    sc_pid pid = execute_server(server, params);
    if (pid == SC_PROCESS_NONE) {
        sc_adb_tunnel_close(&server->tunnel, &server->intr, params->serial);
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
        sc_adb_tunnel_close(&server->tunnel, &server->intr, params->serial);
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

    return 0;

error_connection_failed:
    server->cbs->on_connection_failed(server, server->cbs_userdata);
    return -1;
}

bool
sc_server_start(struct sc_server *server) {
    bool ok = sc_thread_create(&server->thread, run_server, "server", server);
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

    sc_thread_join(&server->thread, NULL);
}

void
sc_server_destroy(struct sc_server *server) {
    sc_server_params_destroy(&server->params);
    sc_intr_destroy(&server->intr);
    sc_cond_destroy(&server->cond_stopped);
    sc_mutex_destroy(&server->mutex);
}
