#ifndef SC_MOUSE_UHID_H
#define SC_MOUSE_UHID_H

#include <stdbool.h>

#include "controller.h"
#include "trait/mouse_processor.h"

struct sc_mouse_uhid {
    struct sc_mouse_processor mouse_processor; // mouse processor trait

    struct sc_controller *controller;
};

bool
sc_mouse_uhid_init(struct sc_mouse_uhid *mouse,
                   struct sc_controller *controller);

#endif
