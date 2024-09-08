#ifndef SC_GAMEPAD_RESAMPLER_H
#define SC_GAMEPAD_RESAMPLER_H

#include "common.h"

#include <stdbool.h>

#include "hid/hid_gamepad.h" // for SC_MAX_GAMEPADS
#include "trait/gamepad_processor.h"
#include "util/thread.h"
#include "util/tick.h"

struct sc_gamepad_resampler_slot {
    uint32_t gamepad_id;
    sc_tick deadline;
    uint32_t buttons;
    uint16_t axis_left_x;
    uint16_t axis_left_y;
    uint16_t axis_right_x;
    uint16_t axis_right_y;
    uint16_t axis_left_trigger;
    uint16_t axis_right_trigger;
};

struct sc_gamepad_resampler {
    struct sc_gamepad_processor gamepad_processor; // gamepad processor trait

    struct sc_gamepad_processor *delegate;
    sc_tick min_interval;

    sc_mutex mutex;
    sc_cond cond;
    sc_thread thread;
    bool stopped;

    struct sc_gamepad_resampler_slot slots[SC_MAX_GAMEPADS];
};

bool
sc_gamepad_resampler_init(struct sc_gamepad_resampler *resampler,
                          struct sc_gamepad_processor *delegate,
                          sc_tick min_interval);

void
sc_gamepad_resampler_stop(struct sc_gamepad_resampler *resampler);

void
sc_gamepad_resampler_join(struct sc_gamepad_resampler *resampler);

void
sc_gamepad_resampler_destroy(struct sc_gamepad_resampler *resampler);

#endif
