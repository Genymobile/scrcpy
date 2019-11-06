#ifndef SCRCPY_H
#define SCRCPY_H

#include <stdbool.h>
#include <stdint.h>
#include <recorder.h>

#include "config.h"

struct scrcpy_options {
    const char *serial;
    const char *crop;
    const char *record_filename;
    const char *window_title;
    const char *push_target;
    enum recorder_format record_format;
    uint16_t port;
    uint16_t max_size;
    uint32_t bit_rate;
    bool show_touches;
    bool fullscreen;
    bool always_on_top;
    bool control;
    bool display;
    bool turn_screen_off;
    bool render_expired_frames;
};

#define SCRCPY_OPTIONS_DEFAULT { \
    .serial = NULL, \
    .crop = NULL, \
    .record_filename = NULL, \
    .window_title = NULL, \
    .push_target = NULL, \
    .record_format = RECORDER_FORMAT_AUTO, \
    .port = DEFAULT_LOCAL_PORT, \
    .max_size = DEFAULT_LOCAL_PORT, \
    .bit_rate = DEFAULT_BIT_RATE, \
    .show_touches = false, \
    .fullscreen = false, \
    .always_on_top = false, \
    .control = true, \
    .display = true, \
    .turn_screen_off = false, \
    .render_expired_frames = false, \
}

bool
scrcpy(const struct scrcpy_options *options);

#endif
