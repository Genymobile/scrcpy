#ifndef SC_KEYBOARD_UHID_H
#define SC_KEYBOARD_UHID_H

#include "common.h"

#include <stdbool.h>

#include "controller.h"
#include "hid/hid_keyboard.h"
#include "uhid/uhid_output.h"
#include "trait/key_processor.h"

struct sc_keyboard_uhid {
    struct sc_key_processor key_processor; // key processor trait
    struct sc_uhid_receiver uhid_receiver;

    struct sc_hid_keyboard hid;
    struct sc_controller *controller;
    atomic_uint_least16_t device_mod;
};

bool
sc_keyboard_uhid_init(struct sc_keyboard_uhid *kb,
                      struct sc_controller *controller,
                      struct sc_uhid_devices *uhid_devices);

#endif
