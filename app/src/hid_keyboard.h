#ifndef SC_HID_KEYBOARD_H
#define SC_HID_KEYBOARD_H

#include "common.h"

#include <stdbool.h>

#include "aoa_hid.h"
#include "trait/key_processor.h"

// See "SDL2/SDL_scancode.h".
// Maybe SDL_Keycode is used by most people, but SDL_Scancode is taken from USB
// HID protocol.
// 0x65 is Application, typically AT-101 Keyboard ends here.
#define SC_HID_KEYBOARD_KEYS 0x66

/**
 * HID keyboard events are sequence-based, every time keyboard state changes
 * it sends an array of currently pressed keys, the host is responsible for
 * compare events and determine which key becomes pressed and which key becomes
 * released. In order to convert SDL_KeyboardEvent to HID events, we first use
 * an array of keys to save each keys' state. And when a SDL_KeyboardEvent was
 * emitted, we updated our state, and then we use a loop to generate HID
 * events. The sequence of array elements is unimportant and when too much keys
 * pressed at the same time (more than report count), we should generate
 * phantom state. Don't forget that modifiers should be updated too, even for
 * phantom state.
 */
struct sc_hid_keyboard {
    struct sc_key_processor key_processor; // key processor trait

    struct sc_aoa *aoa;
    bool keys[SC_HID_KEYBOARD_KEYS];

    bool mod_lock_synchronized;
};

bool
sc_hid_keyboard_init(struct sc_hid_keyboard *kb, struct sc_aoa *aoa);

void
sc_hid_keyboard_destroy(struct sc_hid_keyboard *kb);

#endif
