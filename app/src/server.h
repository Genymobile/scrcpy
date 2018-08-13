#ifndef SERVER_H
#define SERVER_H

#include "command.h"
#include "net.h"

struct server {
    const char *serial;
    process_t process;
    socket_t server_socket; // only used if !tunnel_forward
    socket_t device_socket;
    Uint16 local_port;
    SDL_bool tunnel_enabled;
    SDL_bool tunnel_forward; // use "adb forward" instead of "adb reverse"
    SDL_bool server_copied_to_device;
};

#define SERVER_INITIALIZER {              \
    .serial = NULL,                       \
    .process = PROCESS_NONE,              \
    .server_socket = INVALID_SOCKET,      \
    .device_socket = INVALID_SOCKET,      \
    .local_port = 0,                      \
    .tunnel_enabled = SDL_FALSE,          \
    .tunnel_forward = SDL_FALSE,          \
    .server_copied_to_device = SDL_FALSE, \
}

// init default values
void server_init(struct server *server);

// push, enable tunnel et start the server
SDL_bool server_start(struct server *server, const char *serial, Uint16 local_port,
                      Uint16 max_size, Uint32 bit_rate, const char *crop);

// block until the communication with the server is established
socket_t server_connect_to(struct server *server);

// disconnect and kill the server process
void server_stop(struct server *server);

// close and release sockets
void server_destroy(struct server *server);

#endif
