#include "server.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_platform.h>

#include "adb.h"
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
    char *server_path = get_local_file_path(SERVER_FILENAME);
    if (!server_path) {
        LOGE("Could not get local file path, "
             "using " SERVER_FILENAME " from current directory");
        return strdup(SERVER_FILENAME);
    }

    LOGD("Using server (portable): %s", server_path);
#endif

    return server_path;
}

static bool
push_server(const char *serial) {
    char *server_path = get_server_path();
    if (!server_path) {
        return false;
    }
    if (!is_regular_file(server_path)) {
        LOGE("'%s' does not exist or is not a regular file\n", server_path);
        free(server_path);
        return false;
    }
    process_t process = adb_push(serial, server_path, DEVICE_SERVER_PATH);
    free(server_path);
    return process_check_success(process, "adb push", true);
}

static bool
enable_tunnel_reverse(const char *serial, uint16_t local_port) {
    process_t process = adb_reverse(serial, SOCKET_NAME, local_port);
    return process_check_success(process, "adb reverse", true);
}

static bool
disable_tunnel_reverse(const char *serial) {
    process_t process = adb_reverse_remove(serial, SOCKET_NAME);
    return process_check_success(process, "adb reverse --remove", true);
}

static bool
enable_tunnel_forward(const char *serial, uint16_t local_port) {
    process_t process = adb_forward(serial, local_port, SOCKET_NAME);
    return process_check_success(process, "adb forward", true);
}

static bool
disable_tunnel_forward(const char *serial, uint16_t local_port) {
    process_t process = adb_forward_remove(serial, local_port);
    return process_check_success(process, "adb forward --remove", true);
}

static bool
disable_tunnel(struct server *server) {
    if (server->tunnel_forward) {
        return disable_tunnel_forward(server->serial, server->local_port);
    }
    return disable_tunnel_reverse(server->serial);
}

static sc_socket
listen_on_port(uint16_t port) {
#define IPV4_LOCALHOST 0x7F000001
    return net_listen(IPV4_LOCALHOST, port, 1);
}

static bool
enable_tunnel_reverse_any_port(struct server *server,
                               struct sc_port_range port_range) {
    uint16_t port = port_range.first;
    for (;;) {
        if (!enable_tunnel_reverse(server->serial, port)) {
            // the command itself failed, it will fail on any port
            return false;
        }

        // At the application level, the device part is "the server" because it
        // serves video stream and control. However, at the network level, the
        // client listens and the server connects to the client. That way, the
        // client can listen before starting the server app, so there is no
        // need to try to connect until the server socket is listening on the
        // device.
        server->server_socket = listen_on_port(port);
        if (server->server_socket != SC_INVALID_SOCKET) {
            // success
            server->local_port = port;
            return true;
        }

        // failure, disable tunnel and try another port
        if (!disable_tunnel_reverse(server->serial)) {
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
    uint16_t port = port_range.first;
    for (;;) {
        if (enable_tunnel_forward(server->serial, port)) {
            // success
            server->local_port = port;
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

static process_t
execute_server(struct server *server, const struct server_params *params) {
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
    return adb_execute(server->serial, cmd, ARRAY_LEN(cmd));
}

static sc_socket
connect_and_read_byte(uint16_t port) {
    sc_socket socket = net_connect(IPV4_LOCALHOST, port);
    if (socket == SC_INVALID_SOCKET) {
        return SC_INVALID_SOCKET;
    }

    char byte;
    // the connection may succeed even if the server behind the "adb tunnel"
    // is not listening, so read one byte to detect a working connection
    if (net_recv(socket, &byte, 1) != 1) {
        // the server is not listening yet behind the adb tunnel
        net_close(socket);
        return SC_INVALID_SOCKET;
    }
    return socket;
}

static sc_socket
connect_to_server(uint16_t port, uint32_t attempts, uint32_t delay) {
    do {
        LOGD("Remaining connection attempts: %d", (int) attempts);
        sc_socket socket = connect_and_read_byte(port);
        if (socket != SC_INVALID_SOCKET) {
            // it worked!
            return socket;
        }
        if (attempts) {
            SDL_Delay(delay);
        }
    } while (--attempts > 0);
    return SC_INVALID_SOCKET;
}

bool
server_init(struct server *server) {
    server->serial = NULL;
    server->process = PROCESS_NONE;

    bool ok = sc_mutex_init(&server->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&server->process_terminated_cond);
    if (!ok) {
        sc_mutex_destroy(&server->mutex);
        return false;
    }

    server->process_terminated = false;

    server->server_socket = SC_INVALID_SOCKET;
    server->video_socket = SC_INVALID_SOCKET;
    server->control_socket = SC_INVALID_SOCKET;

    server->local_port = 0;

    server->tunnel_enabled = false;
    server->tunnel_forward = false;

    return true;
}

static int
run_wait_server(void *data) {
    struct server *server = data;
    process_wait(server->process, false); // ignore exit code

    sc_mutex_lock(&server->mutex);
    server->process_terminated = true;
    sc_cond_signal(&server->process_terminated_cond);
    sc_mutex_unlock(&server->mutex);

    // no need for synchronization, server_socket is initialized before this
    // thread was created
    if (server->server_socket != SC_INVALID_SOCKET) {
        // Unblock any accept()
        net_interrupt(server->server_socket);
    }

    LOGD("Server terminated");
    return 0;
}

bool
server_start(struct server *server, const struct server_params *params) {
    if (params->serial) {
        server->serial = strdup(params->serial);
        if (!server->serial) {
            return false;
        }
    }

    if (!push_server(params->serial)) {
        /* server->serial will be freed on server_destroy() */
        return false;
    }

    if (!enable_tunnel_any_port(server, params->port_range,
                                params->force_adb_forward)) {
        return false;
    }

    // server will connect to our server socket
    server->process = execute_server(server, params);
    if (server->process == PROCESS_NONE) {
        goto error;
    }

    // If the server process dies before connecting to the server socket, then
    // the client will be stuck forever on accept(). To avoid the problem, we
    // must be able to wake up the accept() call when the server dies. To keep
    // things simple and multiplatform, just spawn a new thread waiting for the
    // server process and calling shutdown()/close() on the server socket if
    // necessary to wake up any accept() blocking call.
    bool ok = sc_thread_create(&server->wait_server_thread, run_wait_server,
                               "wait-server", server);
    if (!ok) {
        process_terminate(server->process);
        process_wait(server->process, true); // ignore exit code
        goto error;
    }

    server->tunnel_enabled = true;

    return true;

error:
    // The server socket (if any) will be closed on server_destroy()

    disable_tunnel(server);

    return false;
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

bool
server_connect_to(struct server *server, struct server_info *info) {
    if (!server->tunnel_forward) {
        server->video_socket = net_accept(server->server_socket);
        if (server->video_socket == SC_INVALID_SOCKET) {
            return false;
        }

        server->control_socket = net_accept(server->server_socket);
        if (server->control_socket == SC_INVALID_SOCKET) {
            // the video_socket will be cleaned up on destroy
            return false;
        }

        // we don't need the server socket anymore
        if (!net_close(server->server_socket)) {
            LOGW("Could not close server socket on connect");
        }
        // Do not attempt to close it again on server_destroy()
        server->server_socket = SC_INVALID_SOCKET;
    } else {
        uint32_t attempts = 100;
        uint32_t delay = 100; // ms
        server->video_socket =
            connect_to_server(server->local_port, attempts, delay);
        if (server->video_socket == SC_INVALID_SOCKET) {
            return false;
        }

        // we know that the device is listening, we don't need several attempts
        server->control_socket =
            net_connect(IPV4_LOCALHOST, server->local_port);
        if (server->control_socket == SC_INVALID_SOCKET) {
            return false;
        }
    }

    // we don't need the adb tunnel anymore
    disable_tunnel(server); // ignore failure
    server->tunnel_enabled = false;

    // The sockets will be closed on stop if device_read_info() fails
    return device_read_info(server->video_socket, info);
}

void
server_stop(struct server *server) {
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

    assert(server->process != PROCESS_NONE);

    if (server->tunnel_enabled) {
        // ignore failure
        disable_tunnel(server);
    }

    // Give some delay for the server to terminate properly
    sc_mutex_lock(&server->mutex);
    bool signaled = false;
    if (!server->process_terminated) {
#define WATCHDOG_DELAY SC_TICK_FROM_SEC(1)
        signaled = sc_cond_timedwait(&server->process_terminated_cond,
                                     &server->mutex,
                                     sc_tick_now() + WATCHDOG_DELAY);
    }
    sc_mutex_unlock(&server->mutex);

    // After this delay, kill the server if it's not dead already.
    // On some devices, closing the sockets is not sufficient to wake up the
    // blocking calls while the device is asleep.
    if (!signaled) {
        // The process is terminated, but not reaped (closed) yet, so its PID
        // is still valid.
        LOGW("Killing the server...");
        process_terminate(server->process);
    }

    sc_thread_join(&server->wait_server_thread, NULL);
    process_close(server->process);
}

void
server_destroy(struct server *server) {
    if (server->server_socket != SC_INVALID_SOCKET) {
        if (!net_close(server->server_socket)) {
            LOGW("Could not close server socket");
        }
    }
    if (server->video_socket != SC_INVALID_SOCKET) {
        if (!net_close(server->video_socket)) {
            LOGW("Could not close video socket");
        }
    }
    if (server->control_socket != SC_INVALID_SOCKET) {
        if (!net_close(server->control_socket)) {
            LOGW("Could not close control socket");
        }
    }
    free(server->serial);
    sc_cond_destroy(&server->process_terminated_cond);
    sc_mutex_destroy(&server->mutex);
}
