#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "command.h"
#include "net.h"

struct server {
    char *serial;
    process_t process;
    socket_t server_socket; // only used if !tunnel_forward
    socket_t device_socket;
    uint16_t local_port;
    bool tunnel_enabled;
    bool tunnel_forward; // use "adb forward" instead of "adb reverse"
    bool send_frame_meta; // request frame PTS to be able to record properly
};

#define SERVER_INITIALIZER {              \
    .serial = NULL,                       \
    .process = PROCESS_NONE,              \
    .server_socket = INVALID_SOCKET,      \
    .device_socket = INVALID_SOCKET,      \
    .local_port = 0,                      \
    .tunnel_enabled = false,          \
    .tunnel_forward = false,          \
    .send_frame_meta = false,         \
}

// init default values
void
server_init(struct server *server);

// push, enable tunnel et start the server
bool
server_start(struct server *server, const char *serial,
                      uint16_t local_port, uint16_t max_size, uint32_t bit_rate,
                      const char *crop, bool send_frame_meta);

// block until the communication with the server is established
socket_t
server_connect_to(struct server *server);

// disconnect and kill the server process
void
server_stop(struct server *server);

// close and release sockets
void
server_destroy(struct server *server);

#endif
