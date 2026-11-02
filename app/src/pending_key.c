#include "pending_key.h"

#include <string.h>

static struct sc_pending_key *
sc_pending_keys_find(struct sc_pending_keys *pks, enum sc_scancode scancode) {
    // SC_SCANCODE_UNKNOWN marks a free slot, so it must never be looked up:
    // it would match the first free slot and hand back a stale entry.
    if (scancode == SC_SCANCODE_UNKNOWN) {
        return NULL;
    }

    for (unsigned i = 0; i < SC_PENDING_KEY_COUNT; ++i) {
        if (pks->keys[i].scancode == scancode) {
            return &pks->keys[i];
        }
    }
    return NULL;
}

void
sc_pending_keys_init(struct sc_pending_keys *pks) {
    // SC_SCANCODE_UNKNOWN is 0, so this marks every slot as free
    memset(pks->keys, 0, sizeof(pks->keys));
}

void
sc_pending_keys_put(struct sc_pending_keys *pks, enum sc_scancode scancode) {
    if (scancode == SC_SCANCODE_UNKNOWN) {
        return;
    }

    struct sc_pending_key *key = sc_pending_keys_find(pks, scancode);
    if (!key) {
        for (unsigned i = 0; i < SC_PENDING_KEY_COUNT; ++i) {
            if (pks->keys[i].scancode == SC_SCANCODE_UNKNOWN) {
                key = &pks->keys[i];
                break;
            }
        }
    }
    if (!key) {
        // no free slot (a key-up was lost): handle this key without
        // remembering it
        return;
    }

    key->scancode = scancode;
    key->action = 0;
}

bool
sc_pending_keys_take(struct sc_pending_keys *pks, enum sc_scancode scancode,
                     struct sc_pending_key *out) {
    struct sc_pending_key *key = sc_pending_keys_find(pks, scancode);
    if (!key) {
        return false;
    }

    *out = *key;
    // clear the whole slot, so that no stale field can be handed out later
    memset(key, 0, sizeof(*key));
    return true;
}

void
sc_pending_keys_mark_action(struct sc_pending_keys *pks,
                            enum sc_scancode scancode,
                            enum android_keycode action) {
    struct sc_pending_key *key = sc_pending_keys_find(pks, scancode);
    if (key) {
        key->action = action;
    }
}
