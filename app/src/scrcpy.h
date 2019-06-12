#ifndef SCRCPY_H
#define SCRCPY_H

#include <stdbool.h>
#include <stdint.h>
#include <recorder.h>

struct scrcpy_options {
    const char *serial;
    const char *crop;
    const char *record_filename;
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

bool
scrcpy(const struct scrcpy_options *options);

#endif
