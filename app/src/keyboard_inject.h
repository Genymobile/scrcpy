#ifndef SC_KEYBOARD_INJECT_H
#define SC_KEYBOARD_INJECT_H

#include "common.h"

#include <stdbool.h>

#include "controller.h"
#include "options.h"
#include "trait/key_processor.h"

struct sc_keyboard_inject {
    struct sc_key_processor key_processor; // key processor trait

    struct sc_controller *controller;

    // SDL reports repeated events as a boolean, but Android expects the actual
    // number of repetitions. This variable keeps track of the count.
    unsigned repeat;

    enum sc_key_inject_mode key_inject_mode;
    bool forward_key_repeat;
};

void
sc_keyboard_inject_init(struct sc_keyboard_inject *ki,
                        struct sc_controller *controller,
                        enum sc_key_inject_mode key_inject_mode,
                        bool forward_key_repeat);

#endif
