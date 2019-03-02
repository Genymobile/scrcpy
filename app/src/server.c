#include "server.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_timer.h>

#include "config.h"
#include "log.h"
#include "net.h"

#define SOCKET_NAME "scrcpy"

#ifdef OVERRIDE_SERVER_PATH
# define DEFAULT_SERVER_PATH OVERRIDE_SERVER_PATH
#else
# define DEFAULT_SERVER_PATH PREFIX PREFIXED_SERVER_PATH
#endif

#define DEVICE_SERVER_PATH "/data/local/tmp/scrcpy-server.jar"

static const char *
get_server_path(void) {
    const char *server_path = getenv("SCRCPY_SERVER_PATH");
    if (!server_path) {
        server_path = DEFAULT_SERVER_PATH;
    }
    return server_path;
}

static bool
push_server(const char *serial) {
    process_t process = adb_push(serial, get_server_path(), DEVICE_SERVER_PATH);
    return process_check_success(process, "adb push");
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
enable_tunnel(struct server *server) {
    if (enable_tunnel_reverse(server->serial, server->local_port)) {
        return true;
    }

    LOGW("'adb reverse' failed, fallback to 'adb forward'");
    server->tunnel_forward = true;
    return enable_tunnel_forward(server->serial, server->local_port);
}

static bool
disable_tunnel(struct server *server) {
    if (server->tunnel_forward) {
        return disable_tunnel_forward(server->serial, server->local_port);
    }
    return disable_tunnel_reverse(server->serial);
}

static process_t
execute_server(const char *serial,
               uint16_t max_size, uint32_t bit_rate,
               bool tunnel_forward, const char *crop,
               bool send_frame_meta) {
    char max_size_string[6];
    char bit_rate_string[11];
    sprintf(max_size_string, "%"PRIu16, max_size);
    sprintf(bit_rate_string, "%"PRIu32, bit_rate);
    const char *const cmd[] = {
        "shell",
        "CLASSPATH=/data/local/tmp/scrcpy-server.jar",
        "app_process",
        "/", // unused
        "com.genymobile.scrcpy.Server",
        max_size_string,
        bit_rate_string,
        tunnel_forward ? "true" : "false",
        crop ? crop : "-",
        send_frame_meta ? "true" : "false",
    };
    return adb_execute(serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

#define IPV4_LOCALHOST 0x7F000001

static socket_t
listen_on_port(uint16_t port) {
    return net_listen(IPV4_LOCALHOST, port, 1);
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
    if (net_recv_all(socket, &byte, 1) != 1) {
        // the server is not listening yet behind the adb tunnel
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
close_socket(socket_t *socket) {
    SDL_assert(*socket != INVALID_SOCKET);
    net_shutdown(*socket, SHUT_RDWR);
    if (!net_close(*socket)) {
        LOGW("Cannot close socket");
        return;
    }
    *socket = INVALID_SOCKET;
}

void
server_init(struct server *server) {
    *server = (struct server) SERVER_INITIALIZER;
}

bool
server_start(struct server *server, const char *serial,
             uint16_t local_port, uint16_t max_size, uint32_t bit_rate,
             const char *crop, bool send_frame_meta) {
    server->local_port = local_port;

    if (serial) {
        server->serial = SDL_strdup(serial);
        if (!server->serial) {
            return false;
        }
    }

    if (!push_server(serial)) {
        SDL_free(server->serial);
        return false;
    }

    if (!enable_tunnel(server)) {
        SDL_free(server->serial);
        return false;
    }

    // if "adb reverse" does not work (e.g. over "adb connect"), it fallbacks to
    // "adb forward", so the app socket is the client
    if (!server->tunnel_forward) {
        // At the application level, the device part is "the server" because it
        // serves video stream and control. However, at the network level, the
        // client listens and the server connects to the client. That way, the
        // client can listen before starting the server app, so there is no
        // need to try to connect until the server socket is listening on the
        // device.

        server->server_socket = listen_on_port(local_port);
        if (server->server_socket == INVALID_SOCKET) {
            LOGE("Could not listen on port %" PRIu16, local_port);
            disable_tunnel(server);
            SDL_free(server->serial);
            return false;
        }
    }

    // server will connect to our server socket
    server->process = execute_server(serial, max_size, bit_rate,
                                     server->tunnel_forward, crop,
                                     send_frame_meta);

    if (server->process == PROCESS_NONE) {
        if (!server->tunnel_forward) {
            close_socket(&server->server_socket);
        }
        disable_tunnel(server);
        SDL_free((void *) server->serial);
        return false;
    }

    server->tunnel_enabled = true;

    return true;
}

socket_t
server_connect_to(struct server *server) {
    if (!server->tunnel_forward) {
        server->device_socket = net_accept(server->server_socket);
    } else {
        uint32_t attempts = 100;
        uint32_t delay = 100; // ms
        server->device_socket = connect_to_server(server->local_port, attempts,
                                                  delay);
    }

    if (server->device_socket == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    if (!server->tunnel_forward) {
        // we don't need the server socket anymore
        close_socket(&server->server_socket);
    }

    // we don't need the adb tunnel anymore
    disable_tunnel(server); // ignore failure
    server->tunnel_enabled = false;

    return server->device_socket;
}

void
server_stop(struct server *server) {
    SDL_assert(server->process != PROCESS_NONE);

    if (!cmd_terminate(server->process)) {
        LOGW("Cannot terminate server");
    }

    cmd_simple_wait(server->process, NULL); // ignore exit code
    LOGD("Server terminated");

    if (server->tunnel_enabled) {
        // ignore failure
        disable_tunnel(server);
    }
}

void
server_destroy(struct server *server) {
    if (server->server_socket != INVALID_SOCKET) {
        close_socket(&server->server_socket);
    }
    if (server->device_socket != INVALID_SOCKET) {
        close_socket(&server->device_socket);
    }
    SDL_free(server->serial);
}
