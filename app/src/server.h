#ifndef SC_SERVER_H
#define SC_SERVER_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "adb/adb_tunnel.h"
#include "options.h"
#include "util/intr.h"
#include "util/net.h"
#include "util/thread.h"
#include "util/tick.h"

#define SC_DEVICE_NAME_FIELD_LENGTH 64
struct sc_server_info {
    char device_name[SC_DEVICE_NAME_FIELD_LENGTH];
};

struct sc_server_params {
    uint32_t scid;
    const char *req_serial;
    enum sc_log_level log_level;
    enum sc_codec video_codec;
    enum sc_codec audio_codec;
    enum sc_video_source video_source;
    enum sc_audio_source audio_source;
    enum sc_camera_facing camera_facing;
    const char *crop;
    const char *video_codec_options;
    const char *audio_codec_options;
    const char *video_encoder;
    const char *audio_encoder;
    const char *camera_id;
    const char *camera_size;
    const char *camera_ar;
    uint16_t camera_fps;
    struct sc_port_range port_range;
    uint32_t tunnel_host;
    uint16_t tunnel_port;
    uint16_t max_size;
    uint32_t video_bit_rate;
    uint32_t audio_bit_rate;
    const char *max_fps; // float to be parsed by the server
    const char *angle; // float to be parsed by the server
    sc_tick screen_off_timeout;
    enum sc_orientation capture_orientation;
    enum sc_orientation_lock capture_orientation_lock;
    bool control;
    uint32_t display_id;
    const char *new_display;
    enum sc_display_ime_policy display_ime_policy;
    bool video;
    bool audio;
    bool audio_dup;
    bool show_touches;
    bool stay_awake;
    bool force_adb_forward;
    bool power_off_on_close;
    bool clipboard_autosync;
    bool downsize_on_error;
    bool tcpip;
    const char *tcpip_dst;
    bool select_usb;
    bool select_tcpip;
    bool cleanup;
    bool power_on;
    bool kill_adb_on_close;
    bool camera_high_speed;
    bool vd_destroy_content;
    bool vd_system_decorations;
    uint8_t list;
};

struct sc_server {
    // The internal allocated strings are copies owned by the server
    struct sc_server_params params;
    char *serial;
    char *device_socket_name;

    sc_thread thread;
    struct sc_server_info info; // initialized once connected

    sc_mutex mutex;
    sc_cond cond_stopped;
    bool stopped;

    struct sc_intr intr;
    struct sc_adb_tunnel tunnel;

    sc_socket video_socket;
    sc_socket audio_socket;
    sc_socket control_socket;

    const struct sc_server_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_server_callbacks {
    /**
     * Called when the server failed to connect
     *
     * If it is called, then on_connected() and on_disconnected() will never be
     * called.
     */
    void (*on_connection_failed)(struct sc_server *server, void *userdata);

    /**
     * Called on server connection
     */
    void (*on_connected)(struct sc_server *server, void *userdata);

    /**
     * Called on server disconnection (after it has been connected)
     */
    void (*on_disconnected)(struct sc_server *server, void *userdata);
};

// init the server with the given params
bool
sc_server_init(struct sc_server *server, const struct sc_server_params *params,
               const struct sc_server_callbacks *cbs, void *cbs_userdata);

// start the server asynchronously
bool
sc_server_start(struct sc_server *server);

// disconnect and kill the server process
void
sc_server_stop(struct sc_server *server);

// join the server thread
void
sc_server_join(struct sc_server *server);

// close and release sockets
void
sc_server_destroy(struct sc_server *server);

#endif
