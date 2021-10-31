#ifndef SERVER_H
#define SERVER_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "adb.h"
#include "coords.h"
#include "options.h"
#include "util/log.h"
#include "util/net.h"
#include "util/thread.h"

#define DEVICE_NAME_FIELD_LENGTH 64
struct server_info {
    char device_name[DEVICE_NAME_FIELD_LENGTH];
    struct sc_size frame_size;
};

struct server_params {
    const char *serial;
    enum sc_log_level log_level;
    const char *crop;
    const char *codec_options;
    const char *encoder_name;
    struct sc_port_range port_range;
    uint16_t max_size;
    uint32_t bit_rate;
    uint16_t max_fps;
    int8_t lock_video_orientation;
    bool control;
    uint32_t display_id;
    bool show_touches;
    bool stay_awake;
    bool force_adb_forward;
    bool power_off_on_close;
};

struct server {
    // The internal allocated strings are copies owned by the server
    struct server_params params;

    sc_pid process;
    // alive only between start() and stop()
    struct sc_process_observer observer;

    sc_mutex mutex;
    sc_cond cond_stopped;
    bool stopped;

    sc_socket server_socket; // only used if !tunnel_forward
    sc_socket video_socket;
    sc_socket control_socket;
    uint16_t local_port; // selected from port_range
    bool tunnel_enabled;
    bool tunnel_forward; // use "adb forward" instead of "adb reverse"
};

// init the server with the given params
bool
server_init(struct server *server, const struct server_params *params);

// push, enable tunnel et start the server
bool
server_start(struct server *server);

// block until the communication with the server is established
bool
server_connect_to(struct server *server, struct server_info *info);

// disconnect and kill the server process
void
server_stop(struct server *server);

// close and release sockets
void
server_destroy(struct server *server);

#endif
