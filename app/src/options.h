#ifndef SCRCPY_OPTIONS_H
#define SCRCPY_OPTIONS_H

#include "common.h"

#include <assert.h>
#include <stdbool.h>
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
    SC_RECORD_FORMAT_M4A,
    SC_RECORD_FORMAT_MKA,
    SC_RECORD_FORMAT_OPUS,
    SC_RECORD_FORMAT_AAC,
    SC_RECORD_FORMAT_FLAC,
    SC_RECORD_FORMAT_WAV,
};

static inline bool
sc_record_format_is_audio_only(enum sc_record_format fmt) {
    return fmt == SC_RECORD_FORMAT_M4A
        || fmt == SC_RECORD_FORMAT_MKA
        || fmt == SC_RECORD_FORMAT_OPUS
        || fmt == SC_RECORD_FORMAT_AAC
        || fmt == SC_RECORD_FORMAT_FLAC
        || fmt == SC_RECORD_FORMAT_WAV;
}

enum sc_codec {
    SC_CODEC_H264,
    SC_CODEC_H265,
    SC_CODEC_AV1,
    SC_CODEC_OPUS,
    SC_CODEC_AAC,
    SC_CODEC_FLAC,
    SC_CODEC_RAW,
};

enum sc_video_source {
    SC_VIDEO_SOURCE_DISPLAY,
    SC_VIDEO_SOURCE_CAMERA,
};

enum sc_audio_source {
    SC_AUDIO_SOURCE_AUTO, // OUTPUT for video DISPLAY, MIC for video CAMERA
    SC_AUDIO_SOURCE_OUTPUT,
    SC_AUDIO_SOURCE_MIC,
    SC_AUDIO_SOURCE_PLAYBACK,
    SC_AUDIO_SOURCE_MIC_UNPROCESSED,
    SC_AUDIO_SOURCE_MIC_CAMCORDER,
    SC_AUDIO_SOURCE_MIC_VOICE_RECOGNITION,
    SC_AUDIO_SOURCE_MIC_VOICE_COMMUNICATION,
    SC_AUDIO_SOURCE_VOICE_CALL,
    SC_AUDIO_SOURCE_VOICE_CALL_UPLINK,
    SC_AUDIO_SOURCE_VOICE_CALL_DOWNLINK,
    SC_AUDIO_SOURCE_VOICE_PERFORMANCE,
};

enum sc_camera_facing {
    SC_CAMERA_FACING_ANY,
    SC_CAMERA_FACING_FRONT,
    SC_CAMERA_FACING_BACK,
    SC_CAMERA_FACING_EXTERNAL,
};

                              // ,----- hflip (applied before the rotation)
                              // | ,--- 180°
                              // | | ,- 90° clockwise
                              // | | |
enum sc_orientation {         // v v v
    SC_ORIENTATION_0,         // 0 0 0
    SC_ORIENTATION_90,        // 0 0 1
    SC_ORIENTATION_180,       // 0 1 0
    SC_ORIENTATION_270,       // 0 1 1
    SC_ORIENTATION_FLIP_0,    // 1 0 0
    SC_ORIENTATION_FLIP_90,   // 1 0 1
    SC_ORIENTATION_FLIP_180,  // 1 1 0
    SC_ORIENTATION_FLIP_270,  // 1 1 1
};

enum sc_orientation_lock {
    SC_ORIENTATION_UNLOCKED,
    SC_ORIENTATION_LOCKED_VALUE,   // lock to specified orientation
    SC_ORIENTATION_LOCKED_INITIAL, // lock to initial device orientation
};

enum sc_display_ime_policy {
    SC_DISPLAY_IME_POLICY_UNDEFINED,
    SC_DISPLAY_IME_POLICY_LOCAL,
    SC_DISPLAY_IME_POLICY_FALLBACK,
    SC_DISPLAY_IME_POLICY_HIDE,
};

static inline bool
sc_orientation_is_mirror(enum sc_orientation orientation) {
    assert(!(orientation & ~7));
    return orientation & 4;
}

// Does the orientation swap width and height?
static inline bool
sc_orientation_is_swap(enum sc_orientation orientation) {
    assert(!(orientation & ~7));
    return orientation & 1;
}

static inline enum sc_orientation
sc_orientation_get_rotation(enum sc_orientation orientation) {
    assert(!(orientation & ~7));
    return orientation & 3;
}

enum sc_orientation
sc_orientation_apply(enum sc_orientation src, enum sc_orientation transform);

static inline const char *
sc_orientation_get_name(enum sc_orientation orientation) {
    switch (orientation) {
        case SC_ORIENTATION_0:
            return "0";
        case SC_ORIENTATION_90:
            return "90";
        case SC_ORIENTATION_180:
            return "180";
        case SC_ORIENTATION_270:
            return "270";
        case SC_ORIENTATION_FLIP_0:
            return "flip0";
        case SC_ORIENTATION_FLIP_90:
            return "flip90";
        case SC_ORIENTATION_FLIP_180:
            return "flip180";
        case SC_ORIENTATION_FLIP_270:
            return "flip270";
        default:
            return "(unknown)";
    }
}

enum sc_keyboard_input_mode {
    SC_KEYBOARD_INPUT_MODE_AUTO,
    SC_KEYBOARD_INPUT_MODE_UHID_OR_AOA, // normal vs otg mode
    SC_KEYBOARD_INPUT_MODE_DISABLED,
    SC_KEYBOARD_INPUT_MODE_SDK,
    SC_KEYBOARD_INPUT_MODE_UHID,
    SC_KEYBOARD_INPUT_MODE_AOA,
};

enum sc_mouse_input_mode {
    SC_MOUSE_INPUT_MODE_AUTO,
    SC_MOUSE_INPUT_MODE_UHID_OR_AOA, // normal vs otg mode
    SC_MOUSE_INPUT_MODE_DISABLED,
    SC_MOUSE_INPUT_MODE_SDK,
    SC_MOUSE_INPUT_MODE_UHID,
    SC_MOUSE_INPUT_MODE_AOA,
};

enum sc_gamepad_input_mode {
    SC_GAMEPAD_INPUT_MODE_DISABLED,
    SC_GAMEPAD_INPUT_MODE_UHID_OR_AOA, // normal vs otg mode
    SC_GAMEPAD_INPUT_MODE_UHID,
    SC_GAMEPAD_INPUT_MODE_AOA,
};

enum sc_mouse_binding {
    SC_MOUSE_BINDING_AUTO,
    SC_MOUSE_BINDING_DISABLED,
    SC_MOUSE_BINDING_CLICK,
    SC_MOUSE_BINDING_BACK,
    SC_MOUSE_BINDING_HOME,
    SC_MOUSE_BINDING_APP_SWITCH,
    SC_MOUSE_BINDING_EXPAND_NOTIFICATION_PANEL,
};

struct sc_mouse_binding_set {
    enum sc_mouse_binding right_click;
    enum sc_mouse_binding middle_click;
    enum sc_mouse_binding click4;
    enum sc_mouse_binding click5;
};

struct sc_mouse_bindings {
    struct sc_mouse_binding_set pri;
    struct sc_mouse_binding_set sec; // When Shift is pressed
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

enum sc_shortcut_mod {
    SC_SHORTCUT_MOD_LCTRL = 1 << 0,
    SC_SHORTCUT_MOD_RCTRL = 1 << 1,
    SC_SHORTCUT_MOD_LALT = 1 << 2,
    SC_SHORTCUT_MOD_RALT = 1 << 3,
    SC_SHORTCUT_MOD_LSUPER = 1 << 4,
    SC_SHORTCUT_MOD_RSUPER = 1 << 5,
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
    const char *video_codec_options;
    const char *audio_codec_options;
    const char *video_encoder;
    const char *audio_encoder;
    const char *camera_id;
    const char *camera_size;
    const char *camera_ar;
    uint16_t camera_fps;
    enum sc_log_level log_level;
    enum sc_codec video_codec;
    enum sc_codec audio_codec;
    enum sc_video_source video_source;
    enum sc_audio_source audio_source;
    enum sc_record_format record_format;
    enum sc_keyboard_input_mode keyboard_input_mode;
    enum sc_mouse_input_mode mouse_input_mode;
    enum sc_gamepad_input_mode gamepad_input_mode;
    struct sc_mouse_bindings mouse_bindings;
    enum sc_camera_facing camera_facing;
    struct sc_port_range port_range;
    uint32_t tunnel_host;
    uint16_t tunnel_port;
    uint8_t shortcut_mods; // OR of enum sc_shortcut_mod values
    uint16_t max_size;
    uint32_t video_bit_rate;
    uint32_t audio_bit_rate;
    const char *max_fps; // float to be parsed by the server
    const char *angle; // float to be parsed by the server
    enum sc_orientation capture_orientation;
    enum sc_orientation_lock capture_orientation_lock;
    enum sc_orientation display_orientation;
    enum sc_orientation record_orientation;
    enum sc_display_ime_policy display_ime_policy;
    int16_t window_x; // SC_WINDOW_POSITION_UNDEFINED for "auto"
    int16_t window_y; // SC_WINDOW_POSITION_UNDEFINED for "auto"
    uint16_t window_width;
    uint16_t window_height;
    uint32_t display_id;
    sc_tick video_buffer;
    sc_tick audio_buffer;
    sc_tick audio_output_buffer;
    sc_tick time_limit;
    sc_tick screen_off_timeout;
#ifdef HAVE_V4L2
    const char *v4l2_device;
    sc_tick v4l2_buffer;
#endif
#ifdef HAVE_USB
    bool otg;
#endif
    bool show_touches;
    bool fullscreen;
    bool always_on_top;
    bool control;
    bool video_playback;
    bool audio_playback;
    bool turn_screen_off;
    enum sc_key_inject_mode key_inject_mode;
    bool window_borderless;
    bool mipmaps;
    bool stay_awake;
    bool force_adb_forward;
    bool disable_screensaver;
    bool forward_key_repeat;
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
    bool video;
    bool audio;
    bool require_audio;
    bool kill_adb_on_close;
    bool camera_high_speed;
#define SC_OPTION_LIST_ENCODERS 0x1
#define SC_OPTION_LIST_DISPLAYS 0x2
#define SC_OPTION_LIST_CAMERAS 0x4
#define SC_OPTION_LIST_CAMERA_SIZES 0x8
#define SC_OPTION_LIST_APPS 0x10
    uint8_t list;
    bool window;
    bool mouse_hover;
    bool audio_dup;
    const char *new_display; // [<width>x<height>][/<dpi>] parsed by the server
    const char *start_app;
    bool vd_destroy_content;
    bool vd_system_decorations;
};

extern const struct scrcpy_options scrcpy_options_default;

#endif
