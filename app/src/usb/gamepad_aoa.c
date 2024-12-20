#include "gamepad_aoa.h"

#include <stdbool.h>

#include "input_events.h"
#include "util/log.h"

/** Downcast gamepad processor to gamepad_aoa */
#define DOWNCAST(GP) container_of(GP, struct sc_gamepad_aoa, gamepad_processor)

static void
sc_gamepad_processor_process_gamepad_added(struct sc_gamepad_processor *gp,
                                const struct sc_gamepad_device_event *event) {
    struct sc_gamepad_aoa *gamepad = DOWNCAST(gp);

    struct sc_hid_open hid_open;
    if (!sc_hid_gamepad_generate_open(&gamepad->hid, &hid_open,
                                      event->gamepad_id)) {
        return;
    }

    // exit_on_error: false (a gamepad open failure should not exit scrcpy)
    if (!sc_aoa_push_open(gamepad->aoa, &hid_open, false)) {
        LOGW("Could not push AOA HID open (gamepad)");
    }
}

static void
sc_gamepad_processor_process_gamepad_removed(struct sc_gamepad_processor *gp,
                                const struct sc_gamepad_device_event *event) {
    struct sc_gamepad_aoa *gamepad = DOWNCAST(gp);

    struct sc_hid_close hid_close;
    if (!sc_hid_gamepad_generate_close(&gamepad->hid, &hid_close,
                                       event->gamepad_id)) {
        return;
    }

    if (!sc_aoa_push_close(gamepad->aoa, &hid_close)) {
        LOGW("Could not push AOA HID close (gamepad)");
    }
}

static void
sc_gamepad_processor_process_gamepad_axis(struct sc_gamepad_processor *gp,
                                const struct sc_gamepad_axis_event *event) {
    struct sc_gamepad_aoa *gamepad = DOWNCAST(gp);

    struct sc_hid_input hid_input;
    if (!sc_hid_gamepad_generate_input_from_axis(&gamepad->hid, &hid_input,
                                                 event)) {
        return;
    }

    if (!sc_aoa_push_input(gamepad->aoa, &hid_input)) {
        LOGW("Could not push AOA HID input (gamepad axis)");
    }
}

static void
sc_gamepad_processor_process_gamepad_button(struct sc_gamepad_processor *gp,
                                const struct sc_gamepad_button_event *event) {
    struct sc_gamepad_aoa *gamepad = DOWNCAST(gp);

    struct sc_hid_input hid_input;
    if (!sc_hid_gamepad_generate_input_from_button(&gamepad->hid, &hid_input,
                                                   event)) {
        return;
    }

    if (!sc_aoa_push_input(gamepad->aoa, &hid_input)) {
        LOGW("Could not push AOA HID input (gamepad button)");
    }
}

void
sc_gamepad_aoa_init(struct sc_gamepad_aoa *gamepad, struct sc_aoa *aoa) {
    gamepad->aoa = aoa;

    sc_hid_gamepad_init(&gamepad->hid);

    static const struct sc_gamepad_processor_ops ops = {
        .process_gamepad_added = sc_gamepad_processor_process_gamepad_added,
        .process_gamepad_removed = sc_gamepad_processor_process_gamepad_removed,
        .process_gamepad_axis = sc_gamepad_processor_process_gamepad_axis,
        .process_gamepad_button = sc_gamepad_processor_process_gamepad_button,
    };

    gamepad->gamepad_processor.ops = &ops;
}

void
sc_gamepad_aoa_destroy(struct sc_gamepad_aoa *gamepad) {
    (void) gamepad;
    // Do nothing, gamepad->aoa will automatically unregister all devices
}
