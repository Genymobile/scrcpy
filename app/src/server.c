#include "server.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_platform.h>

#include "config.h"
#include "command.h"
#include "util/log.h"
#include "util/net.h"
#include "util/str_util.h"

#define SOCKET_NAME "scrcpy"
#define SSH_SOCKET_NAME "/dev/socket/scrcpy"
#define SERVER_FILENAME "scrcpy-server"
#define ABSTRACTCAT_FILENAME "abstractcat"

#define DEFAULT_SERVER_PATH PREFIX "/share/scrcpy/" SERVER_FILENAME
#define DEVICE_SERVER_PATH "/data/local/tmp/scrcpy-server.jar"

#define DEFAULT_ABSTRACTCAT_PATH PREFIX "/share/scrcpy/" ABSTRACTCAT_FILENAME
#define DEVICE_ABSTRACTCAT_PATH "/data/local/tmp/" ABSTRACTCAT_FILENAME

static void close_socket(socket_t socket);

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
        char *server_path = SDL_strdup(server_path_env);
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
    char *server_path = SDL_strdup(DEFAULT_SERVER_PATH);
    if (!server_path) {
        LOGE("Could not allocate memory");
        return NULL;
    }
    // the absolute path is hardcoded
    return server_path;
#else

    // use scrcpy-server in the same directory as the executable
    char *executable_path = get_executable_path();
    if (!executable_path) {
        LOGE("Could not get executable path, "
             "using " SERVER_FILENAME " from current directory");
        // not found, use current directory
        return SERVER_FILENAME;
    }
    char *dir = dirname(executable_path);
    size_t dirlen = strlen(dir);

    // sizeof(SERVER_FILENAME) gives statically the size including the null byte
    size_t len = dirlen + 1 + sizeof(SERVER_FILENAME);
    char *server_path = SDL_malloc(len);
    if (!server_path) {
        LOGE("Could not alloc server path string, "
             "using " SERVER_FILENAME " from current directory");
        SDL_free(executable_path);
        return SERVER_FILENAME;
    }

    memcpy(server_path, dir, dirlen);
    server_path[dirlen] = PATH_SEPARATOR;
    memcpy(&server_path[dirlen + 1], SERVER_FILENAME, sizeof(SERVER_FILENAME));
    // the final null byte has been copied with SERVER_FILENAME

    SDL_free(executable_path);

    LOGD("Using server (portable): %s", server_path);
    return server_path;
#endif
}

static char *
get_abstractcat_path(void) {
#ifdef __WINDOWS__
    const wchar_t *abstractcat_path_env = _wgetenv(L"SCRCPY_ABSTRACTCAT_PATH");
#else
    const char *abstractcat_path_env = getenv("SCRCPY_ABSTRACTCAT_PATH");
#endif
    if (abstractcat_path_env) {
        // if the envvar is set, use it
#ifdef __WINDOWS__
        char *abstractcat_path = utf8_from_wide_char(abstractcat_path_env);
#else
        char *abstractcat_path = SDL_strdup(abstractcat_path_env);
#endif
        if (!abstractcat_path) {
            LOGE("Could not allocate memory");
            return NULL;
        }
        LOGD("Using SCRCPY_ABSTRACTCAT_PATH: %s", abstractcat_path);
        return abstractcat_path;
    }

#ifndef PORTABLE
    LOGD("Using abstractcat: " DEFAULT_ABSTRACTCAT_PATH);
    char *abstractcat_path = SDL_strdup(DEFAULT_ABSTRACTCAT_PATH);
    if (!abstractcat_path) {
        LOGE("Could not allocate memory");
        return NULL;
    }
    // the absolute path is hardcoded
    return abstractcat_path;
#else

    // use scrcpy-server in the same directory as the executable
    char *executable_path = get_executable_path();
    if (!executable_path) {
        LOGE("Could not get executable path, "
             "using " ABSTRACTCAT_FILENAME " from current directory");
        // not found, use current directory
        return ABSTRACTCAT_FILENAME;
    }
    char *dir = dirname(executable_path);
    size_t dirlen = strlen(dir);

    // sizeof(ABSTRACTCAT_FILENAME) gives statically the size including the null byte
    size_t len = dirlen + 1 + sizeof(ABSTRACTCAT_FILENAME);
    char *abstractcat_path = SDL_malloc(len);
    if (!abstractcat_path) {
        LOGE("Could not alloc abstractcat path string, "
             "using " ABSTRACTCAT_FILENAME " from current directory");
        SDL_free(executable_path);
        return ABSTRACTCAT_FILENAME;
    }

    memcpy(abstractcat_path, dir, dirlen);
    abstractcat_path[dirlen] = PATH_SEPARATOR;
    memcpy(&abstractcat_path[dirlen + 1], ABSTRACTCAT_FILENAME, sizeof(ABSTRACTCAT_FILENAME));
    // the final null byte has been copied with ABSTRACTCAT_FILENAME

    SDL_free(executable_path);

    LOGD("Using abstractcat (portable): %s", abstractcat_path);
    return abstractcat_path;
#endif
}

static bool
push_abstractcat(const struct server_params *params) {
    char *abstractcat_path = get_abstractcat_path();
    if (!abstractcat_path) {
        return false;
    }
    if (!is_regular_file(abstractcat_path)) {
        LOGE("'%s' does not exist or is not a regular file\n", abstractcat_path);
        SDL_free(abstractcat_path);
        return false;
    }
    process_t process = ssh_push(params->ssh_endpoint, abstractcat_path, DEVICE_ABSTRACTCAT_PATH);
    SDL_free(abstractcat_path);
    return process_check_success(process, "scp");
}

static bool
push_server(const char *serial, const struct server_params *params) {
    char *server_path = get_server_path();
    if (!server_path) {
        return false;
    }
    if (!is_regular_file(server_path)) {
        LOGE("'%s' does not exist or is not a regular file\n", server_path);
        SDL_free(server_path);
        return false;
    }
    process_t process;
    if (params->use_ssh)
        process = ssh_push(params->ssh_endpoint, server_path, DEVICE_SERVER_PATH);
    else
        process = adb_push(serial, server_path, DEVICE_SERVER_PATH);
    SDL_free(server_path);
    return process_check_success(process, "adb push");
}

static bool prepare_ssh_socket_path(const struct server_params *params) {
    const char *const cmd[] = {
        "rm",
        "-f",
        SSH_SOCKET_NAME,
    };
    process_t process = ssh_execute(params->ssh_endpoint, cmd, sizeof(cmd) / sizeof(cmd[0]),
        NULL, 0, NULL, 0);
    return process_check_success(process, "ssh rm -f " SSH_SOCKET_NAME);
}

static bool
enable_tunnel_reverse(const char *serial, uint16_t local_port) {
    process_t process = adb_reverse(serial, SOCKET_NAME, local_port);
    return process_check_success(process, "adb reverse");
}

static bool
disable_tunnel_reverse(const char *serial) {
    process_t process = adb_reverse_remove(serial, SOCKET_NAME);
    return process_check_success(process, "adb reverse --remove");
}

static bool
enable_tunnel_forward(const char *serial, uint16_t local_port) {
    process_t process = adb_forward(serial, local_port, SOCKET_NAME);
    return process_check_success(process, "adb forward");
}

static bool
disable_tunnel_forward(const char *serial, uint16_t local_port) {
    process_t process = adb_forward_remove(serial, local_port);
    return process_check_success(process, "adb forward --remove");
}

static bool
disable_tunnel(struct server *server) {
    if (server->tunnel_forward) {
        return disable_tunnel_forward(server->serial, server->local_port);
    }
    return disable_tunnel_reverse(server->serial);
}

static socket_t
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
        if (server->server_socket != INVALID_SOCKET) {
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
                 port, port + 1);
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
    char display_id_string[6];
    sprintf(max_size_string, "%"PRIu16, params->max_size);
    sprintf(bit_rate_string, "%"PRIu32, params->bit_rate);
    sprintf(max_fps_string, "%"PRIu16, params->max_fps);
    sprintf(lock_video_orientation_string, "%"PRIi8, params->lock_video_orientation);
    sprintf(display_id_string, "%"PRIu16, params->display_id);
    const char *const cmd[] = {
        params->use_ssh ? "ANDROID_DATA=/data" : "shell",
        "CLASSPATH=" DEVICE_SERVER_PATH,
        "/system/bin/app_process",
#ifdef SERVER_DEBUGGER
# define SERVER_DEBUGGER_PORT "5005"
# ifdef SERVER_DEBUGGER_METHOD_NEW
        /* Android 9 and above */
        "-XjdwpProvider:internal -XjdwpOptions:transport=dt_socket,suspend=y,server=y,address="
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
        server->tunnel_forward || params->force_adb_forward ? "true" : "false",
        params->crop ? params->crop : "-",
        "true", // always send frame meta (packet boundaries + timestamp)
        params->control ? "true" : "false",
        display_id_string,
        params->show_touches ? "true" : "false",
        params->stay_awake ? "true" : "false",
        params->codec_options ? params->codec_options : "-",
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
    if (params->use_ssh)
    {
        for (uint16_t port = server->port_range.first;;) {
            server->server_socket = listen_on_port(port);
            if (server->server_socket == INVALID_SOCKET) {
                if (port < server->port_range.last) {
                    LOGW("Could not listen on port %" PRIu16", retrying on %" PRIu16,
                        port, (uint16_t) (port + 1));
                    port++;
                    continue;
                }

                if (server->port_range.first == server->port_range.last) {
                    LOGE("Could not listen on port %" PRIu16, server->port_range.first);
                } else {
                    LOGE("Could not listen on any port in range %" PRIu16 ":%" PRIu16,
                        server->port_range.first, server->port_range.last);
                }
                return PROCESS_NONE;
            }
            server->local_port = port;
            char *tunnel_desc = SDL_strdup(SSH_SOCKET_NAME ":localhost:12345");

            if (params->force_adb_forward)
                sprintf(tunnel_desc, "localhost:%d:" SSH_SOCKET_NAME, port);
            else
                sprintf(tunnel_desc, SSH_SOCKET_NAME ":localhost:%d", port);

            const char *const ssh_options[] = {
                "-o", "ExitOnForwardFailure=yes",
                params->force_adb_forward ? "-L" : "-R", tunnel_desc,
            };

            const char *const prefix_cmd[] = {
                DEVICE_ABSTRACTCAT_PATH,
                params->force_adb_forward ? SSH_SOCKET_NAME : "@" SOCKET_NAME, // Source.
                params->force_adb_forward ? "@" SOCKET_NAME : SSH_SOCKET_NAME, // Destination.
                "2", // Maximum connections to forward.
            };

            if (params->force_adb_forward) {
                close_socket(server->server_socket);
                server->server_socket = INVALID_SOCKET;
                server->tunnel_forward = true;
            }

            return ssh_execute(params->ssh_endpoint, cmd, sizeof(cmd) / sizeof(cmd[0]),
                prefix_cmd, sizeof(prefix_cmd) / sizeof(prefix_cmd[0]),
                ssh_options, sizeof(ssh_options) / sizeof(ssh_options[0]));
        }
    }
    else
        return adb_execute(server->serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

static socket_t
connect_and_read_byte(uint16_t port) {
    socket_t socket = net_connect(IPV4_LOCALHOST, port);
    if (socket == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    char byte;
    // the connection may succeed even if the server behind the "adb tunnel"
    // is not listening, so read one byte to detect a working connection
    if (net_recv(socket, &byte, 1) != 1) {
        // the server is not listening yet behind the adb tunnel
        net_close(socket);
        return INVALID_SOCKET;
    }
    return socket;
}

static socket_t
connect_to_server(uint16_t port, uint32_t attempts, uint32_t delay) {
    do {
        LOGD("Remaining connection attempts: %d", (int) attempts);
        socket_t socket = connect_and_read_byte(port);
        if (socket != INVALID_SOCKET) {
            // it worked!
            return socket;
        }
        if (attempts) {
            SDL_Delay(delay);
        }
    } while (--attempts > 0);
    return INVALID_SOCKET;
}

static void
close_socket(socket_t socket) {
    assert(socket != INVALID_SOCKET);
    net_shutdown(socket, SHUT_RDWR);
    if (!net_close(socket)) {
        LOGW("Could not close socket");
    }
}

void
server_init(struct server *server) {
    *server = (struct server) SERVER_INITIALIZER;
}

static int
run_wait_server(void *data) {
    struct server *server = data;
    cmd_simple_wait(server->process, NULL); // ignore exit code
    // no need for synchronization, server_socket is initialized before this
    // thread was created
    if (server->server_socket != INVALID_SOCKET
            && !atomic_flag_test_and_set(&server->server_socket_closed)) {
        // On Linux, accept() is unblocked by shutdown(), but on Windows, it is
        // unblocked by closesocket(). Therefore, call both (close_socket()).
        close_socket(server->server_socket);
    }
    LOGD("Server terminated");
    return 0;
}

bool
server_start(struct server *server, const char *serial,
             const struct server_params *params) {
    server->port_range = params->port_range;

    if (serial) {
        server->serial = SDL_strdup(serial);
        if (!server->serial) {
            return false;
        }
    }

    server->use_ssh = params->use_ssh;

    if (!push_server(serial, params)) {
        goto error1;
    }

    if (params->use_ssh && !push_abstractcat(params))
        goto error1;

    if (params->use_ssh && !prepare_ssh_socket_path(params))
        goto error1;

    if (!params->use_ssh && !enable_tunnel_any_port(server, params->port_range,
                                params->force_adb_forward)) {
        goto error1;
    }

    // server will connect to our server socket
    server->process = execute_server(server, params);
    if (server->process == PROCESS_NONE) {
        goto error2;
    }

    // If the server process dies before connecting to the server socket, then
    // the client will be stuck forever on accept(). To avoid the problem, we
    // must be able to wake up the accept() call when the server dies. To keep
    // things simple and multiplatform, just spawn a new thread waiting for the
    // server process and calling shutdown()/close() on the server socket if
    // necessary to wake up any accept() blocking call.
    server->wait_server_thread =
        SDL_CreateThread(run_wait_server, "wait-server", server);
    if (!server->wait_server_thread) {
        cmd_terminate(server->process);
        cmd_simple_wait(server->process, NULL); // ignore exit code
        goto error2;
    }

    server->tunnel_enabled = true;

    return true;

error2:
    if (!server->tunnel_forward) {
        bool was_closed =
            atomic_flag_test_and_set(&server->server_socket_closed);
        // the thread is not started, the flag could not be already set
        assert(!was_closed);
        (void) was_closed;
        close_socket(server->server_socket);
    }

    if (!server->use_ssh) {
        disable_tunnel(server);
    }
error1:
    SDL_free(server->serial);
    return false;
}

bool
server_connect_to(struct server *server) {
    if (!server->tunnel_forward) {
        server->video_socket = net_accept(server->server_socket);
        if (server->video_socket == INVALID_SOCKET) {
            return false;
        }

        server->control_socket = net_accept(server->server_socket);
        if (server->control_socket == INVALID_SOCKET) {
            // the video_socket will be cleaned up on destroy
            return false;
        }

        // we don't need the server socket anymore
        if (!atomic_flag_test_and_set(&server->server_socket_closed)) {
            // close it from here
            close_socket(server->server_socket);
            // otherwise, it is closed by run_wait_server()
        }
    } else {
        uint32_t attempts = 100;
        uint32_t delay = 100; // ms
        server->video_socket =
            connect_to_server(server->local_port, attempts, delay);
        if (server->video_socket == INVALID_SOCKET) {
            return false;
        }

        // we know that the device is listening, we don't need several attempts
        server->control_socket =
            net_connect(IPV4_LOCALHOST, server->local_port);
        if (server->control_socket == INVALID_SOCKET) {
            return false;
        }
    }

    if (server->tunnel_enabled) {
        // we don't need the adb tunnel anymore
        if (!server->use_ssh)
            disable_tunnel(server); // ignore failure
        server->tunnel_enabled = false;
    }

    return true;
}

void
server_stop(struct server *server) {
    if (server->server_socket != INVALID_SOCKET
            && !atomic_flag_test_and_set(&server->server_socket_closed)) {
        close_socket(server->server_socket);
    }
    if (server->video_socket != INVALID_SOCKET) {
        close_socket(server->video_socket);
    }
    if (server->control_socket != INVALID_SOCKET) {
        close_socket(server->control_socket);
    }

    assert(server->process != PROCESS_NONE);

    cmd_terminate(server->process);

    if (server->tunnel_enabled && !server->use_ssh) {
        // ignore failure
        disable_tunnel(server);
    }

    SDL_WaitThread(server->wait_server_thread, NULL);
}

void
server_destroy(struct server *server) {
    SDL_free(server->serial);
}
