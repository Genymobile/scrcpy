#ifndef SC_HID_MOUSE_H
#define SC_HID_MOUSE_H

#include "common.h"

#include <stdbool.h>

#include "aoa_hid.h"
#include "trait/mouse_processor.h"

struct sc_hid_mouse {
    struct sc_mouse_processor mouse_processor; // mouse processor trait

    struct sc_aoa *aoa;
};

bool
sc_hid_mouse_init(struct sc_hid_mouse *mouse, struct sc_aoa *aoa);

void
sc_hid_mouse_destroy(struct sc_hid_mouse *mouse);

#endif
