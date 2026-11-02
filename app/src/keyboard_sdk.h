#ifndef SC_KEYBOARD_SDK_H
#define SC_KEYBOARD_SDK_H

#include "common.h"

#include <stdbool.h>

#include "controller.h"
#include "options.h"
#include "trait/key_processor.h"

struct sc_keyboard_sdk {
    struct sc_key_processor key_processor; // key processor trait

    struct sc_controller *controller;

    // SDL reports repeated events as a boolean, but Android expects the actual
    // number of repetitions. This variable keeps track of the count.
    unsigned repeat;

    enum sc_key_inject_mode key_inject_mode;
    bool forward_key_repeat;

    // If set, the device keyboard (IME) composes text from raw key events, and
    // its language is switched whenever the computer input source switches
    // between a keyboard layout and an input method
    bool device_ime;
    // Last known state of the computer input source (only if device_ime)
    bool input_method_known;
    bool input_method;
};

void
sc_keyboard_sdk_init(struct sc_keyboard_sdk *kb,
                     struct sc_controller *controller,
                     enum sc_key_inject_mode key_inject_mode,
                     bool forward_key_repeat,
                     bool device_ime);

#endif
