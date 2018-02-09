#include "server.h"

#include <SDL2/SDL_log.h>
#include <SDL2/SDL_net.h>
#include <errno.h>
#include <stdint.h>
#include "netutil.h"

#define SOCKET_NAME "scrcpy"

static SDL_bool push_server(const char *serial) {
    const char *server_path = getenv("SCRCPY_SERVER_JAR");
    if (!server_path) {
        server_path = "scrcpy-server.jar";
    }
    process_t process = adb_push(serial, server_path, "/data/local/tmp/scrcpy-server.jar");
    return process_check_success(process, "adb push");
}

static SDL_bool enable_tunnel(const char *serial, Uint16 local_port) {
    process_t process = adb_reverse(serial, SOCKET_NAME, local_port);
    return process_check_success(process, "adb reverse");
}

static SDL_bool disable_tunnel(const char *serial) {
    process_t process = adb_reverse_remove(serial, SOCKET_NAME);
    return process_check_success(process, "adb reverse --remove");
}

static process_t execute_server(const char *serial, Uint16 max_size, Uint32 bit_rate) {
    char max_size_string[6];
    char bit_rate_string[11];
    sprintf(max_size_string, "%"PRIu16, max_size);
    sprintf(bit_rate_string, "%"PRIu32, bit_rate);
    const char *const cmd[] = {
        "shell",
        "CLASSPATH=/data/local/tmp/scrcpy-server.jar",
        "app_process",
        "/", // unused
        "com.genymobile.scrcpy.ScrCpyServer",
        max_size_string,
        bit_rate_string,
    };
    return adb_execute(serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

static void terminate_server(process_t server) {
    if (!cmd_terminate(server)) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Could not terminate server: %s", strerror(errno));
    }
}

static TCPsocket listen_on_port(Uint16 port) {
    IPaddress addr = {
        .host = INADDR_ANY,
        .port = SDL_SwapBE16(port),
    };
    return SDLNet_TCP_Open(&addr);
}

void server_init(struct server *server) {
    *server = (struct server) SERVER_INITIALIZER;
}

SDL_bool server_start(struct server *server, const char *serial, Uint16 local_port,
                      Uint16 max_size, Uint32 bit_rate) {
    if (!push_server(serial)) {
        return SDL_FALSE;
    }

    if (!enable_tunnel(serial, local_port)) {
        return SDL_FALSE;
    }

    // At the application level, the device part is "the server" because it
    // serves video stream and control. However, at network level, the client
    // listens and the server connects to the client. That way, the client can
    // listen before starting the server app, so there is no need to try to
    // connect until the server socket is listening on the device.

    server->server_socket = listen_on_port(local_port);
    if (!server->server_socket) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not listen on port %" PRIu16, local_port);
        disable_tunnel(serial);
        return SDL_FALSE;
    }

    // server will connect to our server socket
    server->process = execute_server(serial, max_size, bit_rate);
    if (server->process == PROCESS_NONE) {
        SDLNet_TCP_Close(server->server_socket);
        disable_tunnel(serial);
        return SDL_FALSE;
    }

    server->adb_reverse_enabled = SDL_TRUE;

    return SDL_TRUE;
}

TCPsocket server_connect_to(struct server *server, const char *serial) {
    SDL_assert(server->server_socket);
    server->device_socket = server_socket_accept(server->server_socket);

    // we don't need the server socket anymore
    SDLNet_TCP_Close(server->server_socket);
    server->server_socket = NULL;

    // we don't need the adb tunnel anymore
    disable_tunnel(serial); // ignore failure
    server->adb_reverse_enabled = SDL_FALSE;

    return server->device_socket;
}

void server_stop(struct server *server, const char *serial) {
    SDL_assert(server->process != PROCESS_NONE);
    terminate_server(server->process);

    if (server->adb_reverse_enabled) {
        // ignore failure
        disable_tunnel(serial);
    }
}

void server_destroy(struct server *server) {
    if (server->server_socket) {
        SDLNet_TCP_Close(server->server_socket);
    }
    if (server->device_socket) {
        SDLNet_TCP_Close(server->device_socket);
    }
}
