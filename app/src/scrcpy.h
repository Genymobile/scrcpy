#ifndef SCRCPY_H
#define SCRCPY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

enum sc_log_level {
    SC_LOG_LEVEL_DEBUG,
    SC_LOG_LEVEL_INFO,
    SC_LOG_LEVEL_WARN,
    SC_LOG_LEVEL_ERROR,
};

enum sc_record_format {
    SC_RECORD_FORMAT_AUTO,
    SC_RECORD_FORMAT_MP4,
    SC_RECORD_FORMAT_MKV,
};

#define SC_MAX_SHORTCUT_MODS 8

enum sc_shortcut_mod {
    SC_MOD_LCTRL = 1 << 0,
    SC_MOD_RCTRL = 1 << 1,
    SC_MOD_LALT = 1 << 2,
    SC_MOD_RALT = 1 << 3,
    SC_MOD_LSUPER = 1 << 4,
    SC_MOD_RSUPER = 1 << 5,
};

struct sc_shortcut_mods {
    unsigned data[SC_MAX_SHORTCUT_MODS];
    unsigned count;
};

struct sc_port_range {
    uint16_t first;
    uint16_t last;
};

#define SC_WINDOW_POSITION_UNDEFINED (-0x8000)

struct scrcpy_options {
    const char *serial;
    const char *crop;
    const char *record_filename;
    const char *window_title;
    const char *push_target;
    const char *render_driver;
    const char *codec_options;
    const char *encoder_name;
    enum sc_log_level log_level;
    enum sc_record_format record_format;
    struct sc_port_range port_range;
    struct sc_shortcut_mods shortcut_mods;
    uint16_t max_size;
    uint32_t bit_rate;
    uint16_t max_fps;
    int8_t lock_video_orientation;
    uint8_t rotation;
    int16_t window_x; // SC_WINDOW_POSITION_UNDEFINED for "auto"
    int16_t window_y; // SC_WINDOW_POSITION_UNDEFINED for "auto"
    uint16_t window_width;
    uint16_t window_height;
    uint16_t display_id;
    bool show_touches;
    bool fullscreen;
    bool always_on_top;
    bool control;
    bool display;
    bool turn_screen_off;
    bool render_expired_frames;
    bool prefer_text;
    bool window_borderless;
    bool mipmaps;
    bool stay_awake;
    bool force_adb_forward;
    bool disable_screensaver;
    bool forward_key_repeat;
    bool forward_all_clicks;
    bool legacy_paste;
};

#define SCRCPY_OPTIONS_DEFAULT { \
    .serial = NULL, \
    .crop = NULL, \
    .record_filename = NULL, \
    .window_title = NULL, \
    .push_target = NULL, \
    .render_driver = NULL, \
    .codec_options = NULL, \
    .encoder_name = NULL, \
    .log_level = SC_LOG_LEVEL_INFO, \
    .record_format = SC_RECORD_FORMAT_AUTO, \
    .port_range = { \
        .first = DEFAULT_LOCAL_PORT_RANGE_FIRST, \
        .last = DEFAULT_LOCAL_PORT_RANGE_LAST, \
    }, \
    .shortcut_mods = { \
        .data = {SC_MOD_LALT, SC_MOD_LSUPER}, \
        .count = 2, \
    }, \
    .max_size = DEFAULT_MAX_SIZE, \
    .bit_rate = DEFAULT_BIT_RATE, \
    .max_fps = 0, \
    .lock_video_orientation = DEFAULT_LOCK_VIDEO_ORIENTATION, \
    .rotation = 0, \
    .window_x = SC_WINDOW_POSITION_UNDEFINED, \
    .window_y = SC_WINDOW_POSITION_UNDEFINED, \
    .window_width = 0, \
    .window_height = 0, \
    .display_id = 0, \
    .show_touches = false, \
    .fullscreen = false, \
    .always_on_top = false, \
    .control = true, \
    .display = true, \
    .turn_screen_off = false, \
    .render_expired_frames = false, \
    .prefer_text = false, \
    .window_borderless = false, \
    .mipmaps = true, \
    .stay_awake = false, \
    .force_adb_forward = false, \
    .disable_screensaver = false, \
    .forward_key_repeat = true, \
    .forward_all_clicks = false, \
    .legacy_paste = false, \
}

bool
scrcpy(const struct scrcpy_options *options);

#endif
