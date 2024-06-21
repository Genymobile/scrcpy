#include "options.h"

const struct scrcpy_options scrcpy_options_default = {
    .serial = NULL,
    .crop = NULL,
    .record_filename = NULL,
    .window_title = NULL,
    .push_target = NULL,
    .render_driver = NULL,
    .video_codec_options = NULL,
    .audio_codec_options = NULL,
    .video_encoder = NULL,
    .audio_encoder = NULL,
    .camera_id = NULL,
    .camera_size = NULL,
    .camera_ar = NULL,
    .camera_fps = 0,
    .log_level = SC_LOG_LEVEL_INFO,
    .video_codec = SC_CODEC_H264,
    .audio_codec = SC_CODEC_OPUS,
    .video_source = SC_VIDEO_SOURCE_DISPLAY,
    .audio_source = SC_AUDIO_SOURCE_AUTO,
    .record_format = SC_RECORD_FORMAT_AUTO,
    .keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_AUTO,
    .mouse_input_mode = SC_MOUSE_INPUT_MODE_AUTO,
    .mouse_bindings = {
        .right_click = SC_MOUSE_BINDING_AUTO,
        .middle_click = SC_MOUSE_BINDING_AUTO,
        .click4 = SC_MOUSE_BINDING_AUTO,
        .click5 = SC_MOUSE_BINDING_AUTO,
    },
    .camera_facing = SC_CAMERA_FACING_ANY,
    .port_range = {
        .first = DEFAULT_LOCAL_PORT_RANGE_FIRST,
        .last = DEFAULT_LOCAL_PORT_RANGE_LAST,
    },
    .tunnel_host = 0,
    .tunnel_port = 0,
    .shortcut_mods = SC_SHORTCUT_MOD_LALT | SC_SHORTCUT_MOD_LSUPER,
    .max_size = 0,
    .video_bit_rate = 0,
    .audio_bit_rate = 0,
    .max_fps = 0,
    .lock_video_orientation = SC_LOCK_VIDEO_ORIENTATION_UNLOCKED,
    .display_orientation = SC_ORIENTATION_0,
    .record_orientation = SC_ORIENTATION_0,
    .window_x = SC_WINDOW_POSITION_UNDEFINED,
    .window_y = SC_WINDOW_POSITION_UNDEFINED,
    .window_width = 0,
    .window_height = 0,
    .display_id = 0,
    .display_buffer = 0,
    .audio_buffer = -1, // depends on the audio format,
    .audio_output_buffer = SC_TICK_FROM_MS(5),
    .time_limit = 0,
#ifdef HAVE_V4L2
    .v4l2_device = NULL,
    .v4l2_buffer = 0,
#endif
#ifdef HAVE_USB
    .otg = false,
#endif
    .show_touches = false,
    .fullscreen = false,
    .always_on_top = false,
    .control = true,
    .video_playback = true,
    .audio_playback = true,
    .turn_screen_off = false,
    .key_inject_mode = SC_KEY_INJECT_MODE_MIXED,
    .window_borderless = false,
    .mipmaps = true,
    .stay_awake = false,
    .force_adb_forward = false,
    .disable_screensaver = false,
    .forward_key_repeat = true,
    .legacy_paste = false,
    .power_off_on_close = false,
    .clipboard_autosync = true,
    .downsize_on_error = true,
    .tcpip = false,
    .tcpip_dst = NULL,
    .select_tcpip = false,
    .select_usb = false,
    .cleanup = true,
    .start_fps_counter = false,
    .power_on = true,
    .video = true,
    .audio = true,
    .require_audio = false,
    .kill_adb_on_close = false,
    .camera_high_speed = false,
    .list = 0,
    .window = true,
    .mouse_hover = true,
};

enum sc_orientation
sc_orientation_apply(enum sc_orientation src, enum sc_orientation transform) {
    assert(!(src & ~7));
    assert(!(transform & ~7));

    unsigned transform_hflip = transform & 4;
    unsigned transform_rotation = transform & 3;
    unsigned src_hflip = src & 4;
    unsigned src_rotation = src & 3;
    unsigned src_swap = src & 1;
    if (src_swap && transform_hflip) {
        // If the src is rotated by 90 or 270 degrees, applying a flipped
        // transformation requires an additional 180 degrees rotation to
        // compensate for the inversion of the order of multiplication:
        //
        //     hflip1 × rotate1 × hflip2 × rotate2
        //     `--------------'   `--------------'
        //           src             transform
        //
        // In the final result, we want all the hflips then all the rotations,
        // so we must move hflip2 to the left:
        //
        //     hflip1 × hflip2 × rotate1' × rotate2
        //
        // with rotate1' = | rotate1           if src is 0° or 180°
        //                 | rotate1 + 180°    if src is 90° or 270°

        src_rotation += 2;
    }

    unsigned result_hflip = src_hflip ^ transform_hflip;
    unsigned result_rotation = (transform_rotation + src_rotation) % 4;
    enum sc_orientation result = result_hflip | result_rotation;
    return result;
}
