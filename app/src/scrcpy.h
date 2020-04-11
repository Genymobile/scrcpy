#ifndef SCRCPY_H
#define SCRCPY_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "common.h"
#include "input_manager.h"
#include "recorder.h"

struct scrcpy_options {
    const char *serial;
    const char *crop;
    const char *record_filename;
    const char *window_title;
    const char *push_target;
    const char *render_driver;
    enum recorder_format record_format;
    struct port_range port_range;
    uint16_t max_size;
    uint32_t bit_rate;
    uint16_t max_fps;
    int8_t lock_video_orientation;
    uint8_t rotation;
    int16_t window_x; // WINDOW_POSITION_UNDEFINED for "auto"
    int16_t window_y; // WINDOW_POSITION_UNDEFINED for "auto"
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
};

#define SCRCPY_OPTIONS_DEFAULT { \
    .serial = NULL, \
    .crop = NULL, \
    .record_filename = NULL, \
    .window_title = NULL, \
    .push_target = NULL, \
    .render_driver = NULL, \
    .record_format = RECORDER_FORMAT_AUTO, \
    .port_range = { \
        .first = DEFAULT_LOCAL_PORT_RANGE_FIRST, \
        .last = DEFAULT_LOCAL_PORT_RANGE_LAST, \
    }, \
    .max_size = DEFAULT_MAX_SIZE, \
    .bit_rate = DEFAULT_BIT_RATE, \
    .max_fps = 0, \
    .lock_video_orientation = DEFAULT_LOCK_VIDEO_ORIENTATION, \
    .rotation = 0, \
    .window_x = WINDOW_POSITION_UNDEFINED, \
    .window_y = WINDOW_POSITION_UNDEFINED, \
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
}

bool
scrcpy(const struct scrcpy_options *options);

#endif
