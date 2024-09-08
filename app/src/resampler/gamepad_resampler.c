#include "gamepad_resampler.h"

#include <assert.h>

#include "input_events.h"
#include "util/log.h"

/** Downcast gamepad processor to sc_gamepad_resampler */
#define DOWNCAST(GP) \
    container_of(GP, struct sc_gamepad_resampler, gamepad_processor)

static void
sc_gamepad_processor_slot_init(struct sc_gamepad_resampler_slot *slot,
                               uint32_t gamepad_id) {
    assert(gamepad_id != SC_GAMEPAD_ID_INVALID);
    slot->gamepad_id = gamepad_id;
    slot->buttons = 0;
    slot->axis_left_x = 0;
    slot->axis_left_y = 0;
    slot->axis_right_x = 0;
    slot->axis_right_y = 0;
    slot->axis_left_trigger = 0;
    slot->axis_right_trigger = 0;
}

static ssize_t
sc_gamepad_processor_slot_find(struct sc_gamepad_resampler *resampler,
                               uint32_t gamepad_id) {
    for (size_t i = 0; i < SC_MAX_GAMEPADS; ++i) {
        if (gamepad_id == resampler->slots[i].gamepad_id) {
            // found
            return i;
        }
    }

    return -1;
}

static void
sc_gamepad_processor_process_gamepad_device(struct sc_gamepad_processor *gp,
                                const struct sc_gamepad_device_event *event) {
    struct sc_gamepad_resampler *resampler = DOWNCAST(gp);

    if (event->type == SC_GAMEPAD_DEVICE_ADDED) {

    } else {
        assert(event->type == SC_GAMEPAD_DEVICE_REMOVED);
    }
}

static void
sc_gamepad_processor_process_gamepad_axis(struct sc_gamepad_processor *gp,
                                const struct sc_gamepad_axis_event *event) {
    struct sc_gamepad_resampler *resampler = DOWNCAST(gp);

}

static void
sc_gamepad_processor_process_gamepad_button(struct sc_gamepad_processor *gp,
                                const struct sc_gamepad_button_event *event) {
    struct sc_gamepad_resampler *resampler = DOWNCAST(gp);

}

static void
sc_gamepad_resampler_trigger(void *userdata) {
    struct sc_gamepad_resampler *resampler = userdata;

    sc_tick now = sc_tick_now();

    for (size_t i = 0; i < SC_MAX_GAMEPADS; ++i) {
        struct sc_gamepad_resampler_slot *slot = &resampler->slots[i];
        if (slot->gamepad_id != SC_GAMEPAD_ID_INVALID
                && slot->deadline != SC_TICK_INVALID
                && slot->deadline - now >= resampler->min_interval) {

        }
    }
}

static int
run_resampler(void *data) {

    return 0;
}

bool
sc_gamepad_resampler_init(struct sc_gamepad_resampler *resampler,
                          struct sc_gamepad_processor *delegate,
                          sc_tick min_interval) {
    bool ok = sc_mutex_init(&resampler->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&resampler->cond);
    if (!ok) {
        goto error_destroy_mutex;
    }

    ok = sc_thread_create(&resampler->thread, run_resampler, "scrcpy-gp-rs",
                          resampler);
    if (!ok) {
        LOGE("Could not start gamepad resampler thread");
        goto error_destroy_cond;
    }

    static const struct sc_gamepad_processor_ops ops = {
        .process_gamepad_device = sc_gamepad_processor_process_gamepad_device,
        .process_gamepad_axis = sc_gamepad_processor_process_gamepad_axis,
        .process_gamepad_button = sc_gamepad_processor_process_gamepad_button,
    };

    resampler->gamepad_processor.ops = &ops;

    resampler->delegate = delegate;
    resampler->min_interval = min_interval;
    resampler->stopped = false;

    for (size_t i = 0; i < SC_MAX_GAMEPADS; ++i) {
        resampler->slots[i].gamepad_id = SC_GAMEPAD_ID_INVALID;
    }

    return true;

error_destroy_mutex:
    sc_mutex_destroy(&resampler->mutex);
error_destroy_cond:
    sc_cond_destroy(&resampler->cond);

    return false;
}

void
sc_gamepad_resampler_stop(struct sc_gamepad_resampler *resampler);

void
sc_gamepad_resampler_join(struct sc_gamepad_resampler *resampler);

void
sc_gamepad_resampler_destroy(struct sc_gamepad_resampler *resampler);
