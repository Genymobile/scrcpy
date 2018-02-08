#ifndef SERVER_H
#define SERVER_H

#include <SDL2/SDL_net.h>
#include "command.h"

struct server {
    process_t process;
    TCPsocket server_socket;
    TCPsocket device_socket;
    SDL_bool adb_reverse_enabled;
};

#define SERVER_INITIALIZER {          \
    .process = PROCESS_NONE,          \
    .server_socket = NULL,            \
    .device_socket = NULL,            \
    .adb_reverse_enabled = SDL_FALSE, \
}

// init default values
void server_init(struct server *server);

// push, enable tunnel et start the server
SDL_bool server_start(struct server *server, const char *serial, Uint16 local_port,
                      Uint16 max_size, Uint32 bit_rate);

// block until the communication with the server is established
TCPsocket server_connect_to(struct server *server, const char *serial);

// disconnect and kill the server process
void server_stop(struct server *server, const char *serial);

#endif
