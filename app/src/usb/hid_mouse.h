#ifndef SC_HID_MOUSE_H
#define SC_HID_MOUSE_H

#include "common.h"

#include <stdbool.h>

#include "aoa_hid.h"
#include "trait/hid_interface.h"
#include "trait/mouse_processor.h"

struct sc_hid_mouse {
    struct sc_mouse_processor mouse_processor; // mouse processor trait

    struct sc_hid_interface *hid_interface;
};

bool
sc_hid_mouse_init(struct sc_hid_mouse *mouse, struct sc_hid_interface *hid_interface);

void
sc_hid_mouse_destroy(struct sc_hid_mouse *mouse);

#endif
