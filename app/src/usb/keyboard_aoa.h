#ifndef SC_KEYBOARD_AOA_H
#define SC_KEYBOARD_AOA_H

#include "common.h"

#include <stdbool.h>

#include "aoa_hid.h"
#include "hid/hid_keyboard.h"
#include "trait/key_processor.h"

struct sc_keyboard_aoa {
    struct sc_key_processor key_processor; // key processor trait

    struct sc_hid_keyboard hid;
    struct sc_aoa *aoa;

    bool mod_lock_synchronized;
};

bool
sc_keyboard_aoa_init(struct sc_keyboard_aoa *kb, struct sc_aoa *aoa);

void
sc_keyboard_aoa_destroy(struct sc_keyboard_aoa *kb);

#endif
