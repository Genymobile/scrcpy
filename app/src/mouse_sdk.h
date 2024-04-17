#ifndef SC_MOUSE_SDK_H
#define SC_MOUSE_SDK_H

#include "common.h"

#include <stdbool.h>

#include "controller.h"
#include "screen.h"
#include "trait/mouse_processor.h"

struct sc_mouse_sdk {
    struct sc_mouse_processor mouse_processor; // mouse processor trait

    struct sc_controller *controller;
};

void
sc_mouse_sdk_init(struct sc_mouse_sdk *m, struct sc_controller *controller);

#endif
