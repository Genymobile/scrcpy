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
};

bool
sc_keyboard_uhid_init(struct sc_keyboard_uhid *kb,
                      struct sc_controller *controller);

#endif
