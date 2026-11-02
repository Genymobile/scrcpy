#ifndef SC_PENDING_KEY_H
#define SC_PENDING_KEY_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "android/keycodes.h"
#include "input_events.h"

// A key pressed with a shortcut modifier may be released after the modifier
// itself. Its key-up would then carry a modifier state without the shortcut
// mod: it would be handled as an ordinary key and forwarded to the device,
// while the action injected on key-down would stay open. The pending key state
// remembers what happened on key-down, so that the key-up can be handled the
// same way.
//
// Entries are keyed by scancode (the physical key), not by keycode, so that a
// keyboard layout change between key-down and key-up cannot unpair them.
//
// A slot is reused if the same key is pressed again. A lost key-up (for
// example on focus loss) leaves a stale entry, which is dropped on disconnect
// and otherwise overwritten by the next press of that key. When the table is
// full, the key is handled without being remembered, i.e. as before this
// mechanism existed: evicting an entry could drop a chord that is still held.
struct sc_pending_key {
    enum sc_scancode scancode; // SC_SCANCODE_UNKNOWN if the slot is free
    enum android_keycode action; // action injected on key-down
};

#define SC_PENDING_KEY_COUNT 16

struct sc_pending_keys {
    struct sc_pending_key keys[SC_PENDING_KEY_COUNT];
};

void
sc_pending_keys_init(struct sc_pending_keys *pks);

// Remember a key-down. Ignored if the scancode is unknown.
void
sc_pending_keys_put(struct sc_pending_keys *pks, enum sc_scancode scancode);

// Retrieve and free the entry for a key-up. Returns false if there is none,
// including for an unknown scancode.
bool
sc_pending_keys_take(struct sc_pending_keys *pks, enum sc_scancode scancode,
                     struct sc_pending_key *out);

// Record the action injected for a key-down.
void
sc_pending_keys_mark_action(struct sc_pending_keys *pks,
                            enum sc_scancode scancode,
                            enum android_keycode action);

#endif
