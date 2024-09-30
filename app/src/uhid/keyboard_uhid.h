#ifndef SC_KEYBOARD_UHID_H
#define SC_KEYBOARD_UHID_H

#include "common.h"

#include <stdbool.h>

#include "controller.h"
#include "hid/hid_keyboard.h"
#include "trait/key_processor.h"

struct sc_keyboard_uhid {
    struct sc_key_processor key_processor; // key processor trait

    struct sc_hid_keyboard hid;
    struct sc_controller *controller;
    uint16_t device_mod;
};

bool
sc_keyboard_uhid_init(struct sc_keyboard_uhid *kb,
                      struct sc_controller *controller);

void
sc_keyboard_uhid_process_hid_output(struct sc_keyboard_uhid *kb,
                                    const uint8_t *data, size_t size);

#endif
