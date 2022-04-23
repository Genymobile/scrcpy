#ifndef SCRCPY_OPTIONS_H
#define SCRCPY_OPTIONS_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/tick.h"

enum sc_log_level {
    SC_LOG_LEVEL_VERBOSE,
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

enum sc_lock_video_orientation {
    SC_LOCK_VIDEO_ORIENTATION_UNLOCKED = -1,
    // lock the current orientation when scrcpy starts
    SC_LOCK_VIDEO_ORIENTATION_INITIAL = -2,
    SC_LOCK_VIDEO_ORIENTATION_0 = 0,
    SC_LOCK_VIDEO_ORIENTATION_1,
    SC_LOCK_VIDEO_ORIENTATION_2,
    SC_LOCK_VIDEO_ORIENTATION_3,
};

enum sc_keyboard_input_mode {
    SC_KEYBOARD_INPUT_MODE_INJECT,
    SC_KEYBOARD_INPUT_MODE_HID,
};

enum sc_mouse_input_mode {
    SC_MOUSE_INPUT_MODE_INJECT,
    SC_MOUSE_INPUT_MODE_HID,
};

enum sc_key_inject_mode {
    // Inject special keys, letters and space as key events.
    // Inject numbers and punctuation as text events.
    // This is the default mode.
    SC_KEY_INJECT_MODE_MIXED,

    // Inject special keys as key events.
    // Inject letters and space, numbers and punctuation as text events.
    SC_KEY_INJECT_MODE_TEXT,

    // Inject everything as key events.
    SC_KEY_INJECT_MODE_RAW,
};

#define SC_MAX_SHORTCUT_MODS 8

enum sc_shortcut_mod {
    SC_SHORTCUT_MOD_LCTRL = 1 << 0,
    SC_SHORTCUT_MOD_RCTRL = 1 << 1,
    SC_SHORTCUT_MOD_LALT = 1 << 2,
    SC_SHORTCUT_MOD_RALT = 1 << 3,
    SC_SHORTCUT_MOD_LSUPER = 1 << 4,
    SC_SHORTCUT_MOD_RSUPER = 1 << 5,
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
#ifdef HAVE_V4L2
    const char *v4l2_device;
#endif
    enum sc_log_level log_level;
    enum sc_record_format record_format;
    enum sc_keyboard_input_mode keyboard_input_mode;
    enum sc_mouse_input_mode mouse_input_mode;
    struct sc_port_range port_range;
    uint32_t tunnel_host;
    uint16_t tunnel_port;
    struct sc_shortcut_mods shortcut_mods;
    uint16_t max_size;
    uint32_t bit_rate;
    uint16_t max_fps;
    enum sc_lock_video_orientation lock_video_orientation;
    uint8_t rotation;
    int16_t window_x; // SC_WINDOW_POSITION_UNDEFINED for "auto"
    int16_t window_y; // SC_WINDOW_POSITION_UNDEFINED for "auto"
    uint16_t window_width;
    uint16_t window_height;
    uint32_t display_id;
    sc_tick display_buffer;
    sc_tick v4l2_buffer;
#ifdef HAVE_USB
    bool otg;
#endif
    bool show_touches;
    bool fullscreen;
    bool always_on_top;
    bool control;
    bool display;
    bool turn_screen_off;
    enum sc_key_inject_mode key_inject_mode;
    bool window_borderless;
    bool mipmaps;
    bool stay_awake;
    bool force_adb_forward;
    bool disable_screensaver;
    bool forward_key_repeat;
    bool forward_all_clicks;
    bool legacy_paste;
    bool power_off_on_close;
    bool clipboard_autosync;
    bool downsize_on_error;
    bool tcpip;
    const char *tcpip_dst;
    bool select_usb;
    bool select_tcpip;
    bool cleanup;
    bool start_fps_counter;
    bool power_on;
};

extern const struct scrcpy_options scrcpy_options_default;

#endif
