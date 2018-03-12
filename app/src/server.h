#ifndef SERVER_H
#define SERVER_H

#include "command.h"
#include "net.h"

struct server {
    const char *serial;
    process_t process;
    socket_t server_socket;
    socket_t device_socket;
    SDL_bool adb_reverse_enabled;
    SDL_bool server_copied_to_device;
};

#define SERVER_INITIALIZER {              \
    .serial = NULL,                       \
    .process = PROCESS_NONE,              \
    .server_socket = INVALID_SOCKET,      \
    .device_socket = INVALID_SOCKET,      \
    .adb_reverse_enabled = SDL_FALSE,     \
    .server_copied_to_device = SDL_FALSE, \
}

// init default values
void server_init(struct server *server);

// push, enable tunnel et start the server
SDL_bool server_start(struct server *server, const char *serial, Uint16 local_port,
                      Uint16 max_size, Uint32 bit_rate);

// block until the communication with the server is established
socket_t server_connect_to(struct server *server, Uint32 timeout_ms);

// disconnect and kill the server process
void server_stop(struct server *server);

// close and release sockets
void server_destroy(struct server *server);

#endif
