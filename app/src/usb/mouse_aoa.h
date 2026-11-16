#ifndef SC_MOUSE_AOA_H
#define SC_MOUSE_AOA_H

#include "common.h"

#include <stdbool.h>

#include "usb/aoa_hid.h"
#include "hid/hid_mouse.h"
#include "trait/mouse_processor.h"

struct sc_mouse_aoa {
    struct sc_mouse_processor mouse_processor; // mouse processor trait

    struct sc_hid_mouse hid;
    struct sc_aoa *aoa;
};

bool
sc_mouse_aoa_init(struct sc_mouse_aoa *mouse, struct sc_aoa *aoa);

void
sc_mouse_aoa_destroy(struct sc_mouse_aoa *mouse);

#endif
