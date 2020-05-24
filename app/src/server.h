#ifndef SERVER_H
#define SERVER_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_thread.h>

#include "config.h"
#include "command.h"
#include "common.h"
#include "util/log.h"
#include "util/net.h"

struct server {
    char *serial;
    process_t process;
    SDL_Thread *wait_server_thread;
    atomic_flag server_socket_closed;
    socket_t server_socket; // only used if !tunnel_forward
    socket_t video_socket;
    socket_t control_socket;
    struct port_range port_range;
    uint16_t local_port; // selected from port_range
    bool tunnel_enabled;
    bool tunnel_forward; // use "adb forward" instead of "adb reverse"
};

#define SERVER_INITIALIZER { \
    .serial = NULL, \
    .process = PROCESS_NONE, \
    .wait_server_thread = NULL, \
    .server_socket_closed = ATOMIC_FLAG_INIT, \
    .server_socket = INVALID_SOCKET, \
    .video_socket = INVALID_SOCKET, \
    .control_socket = INVALID_SOCKET, \
    .port_range = { \
        .first = 0, \
        .last = 0, \
    }, \
    .local_port = 0, \
    .tunnel_enabled = false, \
    .tunnel_forward = false, \
}

struct server_params {
    enum sc_log_level log_level;
    const char *crop;
    const char *codec_options;
    struct port_range port_range;
    uint16_t max_size;
    uint32_t bit_rate;
    uint16_t max_fps;
    int8_t lock_video_orientation;
    bool control;
    uint16_t display_id;
    bool show_touches;
    bool stay_awake;
    bool force_adb_forward;
};

// init default values
void
server_init(struct server *server);

// push, enable tunnel et start the server
bool
server_start(struct server *server, const char *serial,
             const struct server_params *params);

// block until the communication with the server is established
bool
server_connect_to(struct server *server);

// disconnect and kill the server process
void
server_stop(struct server *server);

// close and release sockets
void
server_destroy(struct server *server);

#endif
