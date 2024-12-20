#ifndef SC_HID_GAMEPAD_H
#define SC_HID_GAMEPAD_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "hid/hid_event.h"
#include "input_events.h"

#define SC_MAX_GAMEPADS 8
#define SC_HID_ID_GAMEPAD_FIRST 3
#define SC_HID_ID_GAMEPAD_LAST (SC_HID_ID_GAMEPAD_FIRST + SC_MAX_GAMEPADS - 1)

struct sc_hid_gamepad_slot {
    uint32_t gamepad_id;
    uint32_t buttons;
    uint16_t axis_left_x;
    uint16_t axis_left_y;
    uint16_t axis_right_x;
    uint16_t axis_right_y;
    uint16_t axis_left_trigger;
    uint16_t axis_right_trigger;
};

struct sc_hid_gamepad {
    struct sc_hid_gamepad_slot slots[SC_MAX_GAMEPADS];
};

void
sc_hid_gamepad_init(struct sc_hid_gamepad *hid);

bool
sc_hid_gamepad_generate_open(struct sc_hid_gamepad *hid,
                             struct sc_hid_open *hid_open,
                             uint32_t gamepad_id);

bool
sc_hid_gamepad_generate_close(struct sc_hid_gamepad *hid,
                              struct sc_hid_close *hid_close,
                              uint32_t gamepad_id);

bool
sc_hid_gamepad_generate_input_from_button(struct sc_hid_gamepad *hid,
                                          struct sc_hid_input *hid_input,
                                const struct sc_gamepad_button_event *event);

bool
sc_hid_gamepad_generate_input_from_axis(struct sc_hid_gamepad *hid,
                                        struct sc_hid_input *hid_input,
                                const struct sc_gamepad_axis_event *event);

#endif
