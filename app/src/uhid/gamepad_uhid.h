#ifndef SC_GAMEPAD_UHID_H
#define SC_GAMEPAD_UHID_H

#include "common.h"

#include "controller.h"
#include "hid/hid_gamepad.h"
#include "trait/gamepad_processor.h"

struct sc_gamepad_uhid {
    struct sc_gamepad_processor gamepad_processor; // gamepad processor trait

    struct sc_hid_gamepad hid;
    struct sc_controller *controller;
};

void
sc_gamepad_uhid_init(struct sc_gamepad_uhid *mouse,
                     struct sc_controller *controller);

#endif
