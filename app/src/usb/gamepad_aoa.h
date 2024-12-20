#ifndef SC_GAMEPAD_AOA_H
#define SC_GAMEPAD_AOA_H

#include "common.h"

#include "hid/hid_gamepad.h"
#include "usb/aoa_hid.h"
#include "trait/gamepad_processor.h"

struct sc_gamepad_aoa {
    struct sc_gamepad_processor gamepad_processor; // gamepad processor trait

    struct sc_hid_gamepad hid;
    struct sc_aoa *aoa;
};

void
sc_gamepad_aoa_init(struct sc_gamepad_aoa *gamepad, struct sc_aoa *aoa);

void
sc_gamepad_aoa_destroy(struct sc_gamepad_aoa *gamepad);

#endif
