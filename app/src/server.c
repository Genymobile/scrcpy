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
#include "util/net.h"
#include "util/str_util.h"

#define SOCKET_NAME "scrcpy"
#define SERVER_FILENAME "scrcpy-server"

#define DEFAULT_SERVER_PATH PREFIX "/share/scrcpy/" SERVER_FILENAME
#define DEVICE_SERVER_PATH "/data/local/tmp/scrcpy-server.jar"

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
        char *server_path = utf8_from_wide_char(server_path_env);
#else
        char *server_path = strdup(server_path_env);
#endif
        if (!server_path) {
            LOGE("Could not allocate memory");
            return NULL;
        }
        LOGD("Using SCRCPY_SERVER_PATH: %s", server_path);
        return server_path;
    }

#ifndef PORTABLE
    LOGD("Using server: " DEFAULT_SERVER_PATH);
    char *server_path = strdup(DEFAULT_SERVER_PATH);
    if (!server_path) {
        LOGE("Could not allocate memory");
        return NULL;
    }
#else
    char *server_path = sc_file_get_local_path(SERVER_FILENAME);
    if (!server_path) {
        LOGE("Could not get local file path, "
             "using " SERVER_FILENAME " from current directory");
        return strdup(SERVER_FILENAME);
    }

    LOGD("Using server (portable): %s", server_path);
#endif

    return server_path;
}

static void
server_params_destroy(struct server_params *params) {
    // The server stores a copy of the params provided by the user
    free((char *) params->serial);
    free((char *) params->crop);
    free((char *) params->codec_options);
    free((char *) params->encoder_name);
}

static bool
server_params_copy(struct server_params *dst, const struct server_params *src) {
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
#undef COPY

    return true;

error:
    server_params_destroy(dst);
    return false;
}

static bool
push_server(const char *serial) {
    char *server_path = get_server_path();
    if (!server_path) {
        return false;
    }
    if (!sc_file_is_regular(server_path)) {
        LOGE("'%s' does not exist or is not a regular file\n", server_path);
        free(server_path);
        return false;
    }
    sc_pid pid = adb_push(serial, server_path, DEVICE_SERVER_PATH);
    free(server_path);
    return sc_process_check_success(pid, "adb push", true);
}

static bool
enable_tunnel_reverse(const char *serial, uint16_t local_port) {
    sc_pid pid = adb_reverse(serial, SOCKET_NAME, local_port);
    return sc_process_check_success(pid, "adb reverse", true);
}

static bool
disable_tunnel_reverse(const char *serial) {
    sc_pid pid = adb_reverse_remove(serial, SOCKET_NAME);
    return sc_process_check_success(pid, "adb reverse --remove", true);
}

static bool
enable_tunnel_forward(const char *serial, uint16_t local_port) {
    sc_pid pid = adb_forward(serial, local_port, SOCKET_NAME);
    return sc_process_check_success(pid, "adb forward", true);
}

static bool
disable_tunnel_forward(const char *serial, uint16_t local_port) {
    sc_pid pid = adb_forward_remove(serial, local_port);
    return sc_process_check_success(pid, "adb forward --remove", true);
}

static bool
disable_tunnel(struct server *server) {
    assert(server->tunnel_enabled);

    const char *serial = server->params.serial;
    bool ok = server->tunnel_forward
            ? disable_tunnel_forward(serial, server->local_port)
            : disable_tunnel_reverse(serial);

    // Consider tunnel disabled even if the command failed
    server->tunnel_enabled = false;

    return ok;
}

static bool
listen_on_port(sc_socket socket, uint16_t port) {
#define IPV4_LOCALHOST 0x7F000001
    return net_listen(socket, IPV4_LOCALHOST, port, 1);
}

static bool
enable_tunnel_reverse_any_port(struct server *server,
                               struct sc_port_range port_range) {
    const char *serial = server->params.serial;
    uint16_t port = port_range.first;
    for (;;) {
        if (!enable_tunnel_reverse(serial, port)) {
            // the command itself failed, it will fail on any port
            return false;
        }

        // At the application level, the device part is "the server" because it
        // serves video stream and control. However, at the network level, the
        // client listens and the server connects to the client. That way, the
        // client can listen before starting the server app, so there is no
        // need to try to connect until the server socket is listening on the
        // device.
        sc_socket server_socket = net_socket();
        if (server_socket != SC_INVALID_SOCKET) {
            bool ok = listen_on_port(server_socket, port);
            if (ok) {
                // success
                server->server_socket = server_socket;
                server->local_port = port;
                server->tunnel_enabled = true;
                return true;
            }

            net_close(server_socket);
        }

        // failure, disable tunnel and try another port
        if (!disable_tunnel_reverse(serial)) {
            LOGW("Could not remove reverse tunnel on port %" PRIu16, port);
        }

        // check before incrementing to avoid overflow on port 65535
        if (port < port_range.last) {
            LOGW("Could not listen on port %" PRIu16", retrying on %" PRIu16,
                 port, (uint16_t) (port + 1));
            port++;
            continue;
        }

        if (port_range.first == port_range.last) {
            LOGE("Could not listen on port %" PRIu16, port_range.first);
        } else {
            LOGE("Could not listen on any port in range %" PRIu16 ":%" PRIu16,
                 port_range.first, port_range.last);
        }
        return false;
    }
}

static bool
enable_tunnel_forward_any_port(struct server *server,
                               struct sc_port_range port_range) {
    server->tunnel_forward = true;

    const char *serial = server->params.serial;
    uint16_t port = port_range.first;
    for (;;) {
        if (enable_tunnel_forward(serial, port)) {
            // success
            server->local_port = port;
            server->tunnel_enabled = true;
            return true;
        }

        if (port < port_range.last) {
            LOGW("Could not forward port %" PRIu16", retrying on %" PRIu16,
                 port, (uint16_t) (port + 1));
            port++;
            continue;
        }

        if (port_range.first == port_range.last) {
            LOGE("Could not forward port %" PRIu16, port_range.first);
        } else {
            LOGE("Could not forward any port in range %" PRIu16 ":%" PRIu16,
                 port_range.first, port_range.last);
        }
        return false;
    }
}

static bool
enable_tunnel_any_port(struct server *server, struct sc_port_range port_range,
                       bool force_adb_forward) {
    if (!force_adb_forward) {
        // Attempt to use "adb reverse"
        if (enable_tunnel_reverse_any_port(server, port_range)) {
            return true;
        }

        // if "adb reverse" does not work (e.g. over "adb connect"), it
        // fallbacks to "adb forward", so the app socket is the client

        LOGW("'adb reverse' failed, fallback to 'adb forward'");
    }

    return enable_tunnel_forward_any_port(server, port_range);
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

static sc_pid
execute_server(struct server *server, const struct server_params *params) {
    const char *serial = server->params.serial;

    char max_size_string[6];
    char bit_rate_string[11];
    char max_fps_string[6];
    char lock_video_orientation_string[5];
    char display_id_string[11];
    sprintf(max_size_string, "%"PRIu16, params->max_size);
    sprintf(bit_rate_string, "%"PRIu32, params->bit_rate);
    sprintf(max_fps_string, "%"PRIu16, params->max_fps);
    sprintf(lock_video_orientation_string, "%"PRIi8,
            params->lock_video_orientation);
    sprintf(display_id_string, "%"PRIu32, params->display_id);
    const char *const cmd[] = {
        "shell",
        "CLASSPATH=" DEVICE_SERVER_PATH,
        "app_process",
#ifdef SERVER_DEBUGGER
# define SERVER_DEBUGGER_PORT "5005"
# ifdef SERVER_DEBUGGER_METHOD_NEW
        /* Android 9 and above */
        "-XjdwpProvider:internal -XjdwpOptions:transport=dt_socket,suspend=y,"
        "server=y,address="
# else
        /* Android 8 and below */
        "-agentlib:jdwp=transport=dt_socket,suspend=y,server=y,address="
# endif
            SERVER_DEBUGGER_PORT,
#endif
        "/", // unused
        "com.genymobile.scrcpy.Server",
        SCRCPY_VERSION,
        log_level_to_server_string(params->log_level),
        max_size_string,
        bit_rate_string,
        max_fps_string,
        lock_video_orientation_string,
        server->tunnel_forward ? "true" : "false",
        params->crop ? params->crop : "-",
        "true", // always send frame meta (packet boundaries + timestamp)
        params->control ? "true" : "false",
        display_id_string,
        params->show_touches ? "true" : "false",
        params->stay_awake ? "true" : "false",
        params->codec_options ? params->codec_options : "-",
        params->encoder_name ? params->encoder_name : "-",
        params->power_off_on_close ? "true" : "false",
    };
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
    return adb_execute(serial, cmd, ARRAY_LEN(cmd));
}

static bool
connect_and_read_byte(sc_socket socket, uint16_t port) {
    bool ok = net_connect(socket, IPV4_LOCALHOST, port);
    if (!ok) {
        return false;
    }

    char byte;
    // the connection may succeed even if the server behind the "adb tunnel"
    // is not listening, so read one byte to detect a working connection
    if (net_recv(socket, &byte, 1) != 1) {
        // the server is not listening yet behind the adb tunnel
        return false;
    }

    return true;
}

static sc_socket
connect_to_server(struct server *server, uint32_t attempts, sc_tick delay) {
    uint16_t port = server->local_port;
    do {
        LOGD("Remaining connection attempts: %d", (int) attempts);
        sc_socket socket = net_socket();
        if (socket != SC_INVALID_SOCKET) {
            bool ok = connect_and_read_byte(socket, port);
            if (ok) {
                // it worked!
                return socket;
            }

            net_close(socket);
        }
        if (attempts) {
            sc_mutex_lock(&server->mutex);
            sc_tick deadline = sc_tick_now() + delay;
            bool timed_out = false;
            while (!server->stopped && !timed_out) {
                timed_out = !sc_cond_timedwait(&server->cond_stopped,
                                               &server->mutex, deadline);
            }
            bool stopped = server->stopped;
            sc_mutex_unlock(&server->mutex);

            if (stopped) {
                LOGI("Connection attempt stopped");
                break;
            }
        }
    } while (--attempts > 0);
    return SC_INVALID_SOCKET;
}

bool
server_init(struct server *server, const struct server_params *params,
            const struct server_callbacks *cbs, void *cbs_userdata) {
    bool ok = server_params_copy(&server->params, params);
    if (!ok) {
        LOGE("Could not copy server params");
        return false;
    }

    ok = sc_mutex_init(&server->mutex);
    if (!ok) {
        LOGE("Could not create server mutex");
        server_params_destroy(&server->params);
        return false;
    }

    ok = sc_cond_init(&server->cond_stopped);
    if (!ok) {
        LOGE("Could not create server cond_stopped");
        sc_mutex_destroy(&server->mutex);
        server_params_destroy(&server->params);
        return false;
    }

    server->stopped = false;

    server->server_socket = SC_INVALID_SOCKET;
    server->video_socket = SC_INVALID_SOCKET;
    server->control_socket = SC_INVALID_SOCKET;

    server->local_port = 0;

    server->tunnel_enabled = false;
    server->tunnel_forward = false;

    assert(cbs);
    assert(cbs->on_connection_failed);
    assert(cbs->on_connected);
    assert(cbs->on_disconnected);

    server->cbs = cbs;
    server->cbs_userdata = cbs_userdata;

    return true;
}

static bool
device_read_info(sc_socket device_socket, struct server_info *info) {
    unsigned char buf[DEVICE_NAME_FIELD_LENGTH + 4];
    ssize_t r = net_recv_all(device_socket, buf, sizeof(buf));
    if (r < DEVICE_NAME_FIELD_LENGTH + 4) {
        LOGE("Could not retrieve device information");
        return false;
    }
    // in case the client sends garbage
    buf[DEVICE_NAME_FIELD_LENGTH - 1] = '\0';
    memcpy(info->device_name, (char *) buf, sizeof(info->device_name));

    info->frame_size.width = (buf[DEVICE_NAME_FIELD_LENGTH] << 8)
                           | buf[DEVICE_NAME_FIELD_LENGTH + 1];
    info->frame_size.height = (buf[DEVICE_NAME_FIELD_LENGTH + 2] << 8)
                            | buf[DEVICE_NAME_FIELD_LENGTH + 3];
    return true;
}

static bool
server_connect_to(struct server *server, struct server_info *info) {
    assert(server->tunnel_enabled);

    sc_socket video_socket = SC_INVALID_SOCKET;
    sc_socket control_socket = SC_INVALID_SOCKET;
    if (!server->tunnel_forward) {
        video_socket = net_accept(server->server_socket);
        if (video_socket == SC_INVALID_SOCKET) {
            goto fail;
        }

        control_socket = net_accept(server->server_socket);
        if (control_socket == SC_INVALID_SOCKET) {
            goto fail;
        }

        // we don't need the server socket anymore
        if (!net_close(server->server_socket)) {
            LOGW("Could not close server socket on connect");
        }
        // Do not attempt to close it again on server_destroy()
        server->server_socket = SC_INVALID_SOCKET;
    } else {
        uint32_t attempts = 100;
        sc_tick delay = SC_TICK_FROM_MS(100);
        video_socket = connect_to_server(server, attempts, delay);
        if (video_socket == SC_INVALID_SOCKET) {
            goto fail;
        }

        // we know that the device is listening, we don't need several attempts
        control_socket = net_socket();
        if (control_socket == SC_INVALID_SOCKET) {
            goto fail;
        }
        bool ok = net_connect(control_socket, IPV4_LOCALHOST,
                              server->local_port);
        if (!ok) {
            goto fail;
        }
    }

    // we don't need the adb tunnel anymore
    disable_tunnel(server); // ignore failure

    // The sockets will be closed on stop if device_read_info() fails
    bool ok = device_read_info(video_socket, info);
    if (!ok) {
        goto fail;
    }

    assert(video_socket != SC_INVALID_SOCKET);
    assert(control_socket != SC_INVALID_SOCKET);

    server->video_socket = video_socket;
    server->control_socket = control_socket;

    return true;

fail:
    if (video_socket != SC_INVALID_SOCKET) {
        if (!net_close(video_socket)) {
            LOGW("Could not close video socket");
        }
    }

    if (control_socket != SC_INVALID_SOCKET) {
        if (!net_close(control_socket)) {
            LOGW("Could not close control socket");
        }
    }

    // Always leave this function with tunnel disabled
    disable_tunnel(server);

    return false;
}

static void
server_on_terminated(void *userdata) {
    struct server *server = userdata;

    // No need for synchronization, server_socket is initialized before the
    // observer thread is created.
    if (server->server_socket != SC_INVALID_SOCKET) {
        // If the server process dies before connecting to the server socket,
        // then the client will be stuck forever on accept(). To avoid the
        // problem, wake up the accept() call when the server dies.
        net_interrupt(server->server_socket);
    }

    server->cbs->on_disconnected(server, server->cbs_userdata);

    LOGD("Server terminated");
}

static int
run_server(void *data) {
    struct server *server = data;

    const struct server_params *params = &server->params;

    bool ok = push_server(params->serial);
    if (!ok) {
        goto error_connection_failed;
    }

    ok = enable_tunnel_any_port(server, params->port_range,
                                params->force_adb_forward);
    if (!ok) {
        goto error_connection_failed;
    }

    // server will connect to our server socket
    sc_pid pid = execute_server(server, params);
    if (pid == SC_PROCESS_NONE) {
        disable_tunnel(server);
        goto error_connection_failed;
    }

    static const struct sc_process_listener listener = {
        .on_terminated = server_on_terminated,
    };
    struct sc_process_observer observer;
    ok = sc_process_observer_init(&observer, pid, &listener, server);
    if (!ok) {
        sc_process_terminate(pid);
        sc_process_wait(pid, true); // ignore exit code
        disable_tunnel(server);
        goto error_connection_failed;
    }

    ok = server_connect_to(server, &server->info);
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

    // Server stop has been requested
    if (server->server_socket != SC_INVALID_SOCKET) {
        if (!net_interrupt(server->server_socket)) {
            LOGW("Could not interrupt server socket");
        }
    }
    if (server->video_socket != SC_INVALID_SOCKET) {
        if (!net_interrupt(server->video_socket)) {
            LOGW("Could not interrupt video socket");
        }
    }
    if (server->control_socket != SC_INVALID_SOCKET) {
        if (!net_interrupt(server->control_socket)) {
            LOGW("Could not interrupt control socket");
        }
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

    return 0;

error_connection_failed:
    server->cbs->on_connection_failed(server, server->cbs_userdata);
    return -1;
}

bool
server_start(struct server *server) {
    bool ok = sc_thread_create(&server->thread, run_server, "server", server);
    if (!ok) {
        LOGE("Could not create server thread");
        return false;
    }

    return true;
}

void
server_stop(struct server *server) {
    sc_mutex_lock(&server->mutex);
    server->stopped = true;
    sc_cond_signal(&server->cond_stopped);
    sc_mutex_unlock(&server->mutex);

    sc_thread_join(&server->thread, NULL);
}

void
server_destroy(struct server *server) {
    server_params_destroy(&server->params);
    sc_cond_destroy(&server->cond_stopped);
    sc_mutex_destroy(&server->mutex);
}
