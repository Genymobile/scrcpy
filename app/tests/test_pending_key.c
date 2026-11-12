#include "common.h"

#include <assert.h>

#include "pending_key.h"

static void test_put_take(void) {
    struct sc_pending_keys pks;
    sc_pending_keys_init(&pks);

    struct sc_pending_key key;
    assert(!sc_pending_keys_take(&pks, SC_SCANCODE_S, &key));

    sc_pending_keys_put(&pks, SC_SCANCODE_S, SDL_KMOD_LALT);
    sc_pending_keys_mark_action(&pks, SC_SCANCODE_S, AKEYCODE_APP_SWITCH);

    // the entry is readable while the key is down
    const struct sc_pending_key *down = sc_pending_keys_get(&pks, SC_SCANCODE_S);
    assert(down);
    assert(down->mod == SDL_KMOD_LALT);
    assert(down->action == AKEYCODE_APP_SWITCH);
    assert(down->action_sent);
    assert(!down->forwarded);

    assert(sc_pending_keys_take(&pks, SC_SCANCODE_S, &key));
    assert(key.scancode == SC_SCANCODE_S);
    assert(key.mod == SDL_KMOD_LALT);
    assert(key.action == AKEYCODE_APP_SWITCH);
    assert(key.action_sent);

    // the entry is freed by take()
    assert(!sc_pending_keys_take(&pks, SC_SCANCODE_S, &key));
    assert(!sc_pending_keys_get(&pks, SC_SCANCODE_S));
}

static void test_forwarded(void) {
    struct sc_pending_keys pks;
    sc_pending_keys_init(&pks);

    sc_pending_keys_put(&pks, SC_SCANCODE_S, 0);
    sc_pending_keys_mark_forwarded(&pks, SC_SCANCODE_S);

    struct sc_pending_key key;
    assert(sc_pending_keys_take(&pks, SC_SCANCODE_S, &key));
    assert(key.forwarded);
    assert(!key.action_sent);
    assert(key.action == 0);
}

static void test_take_clears_slot(void) {
    struct sc_pending_keys pks;
    sc_pending_keys_init(&pks);

    sc_pending_keys_put(&pks, SC_SCANCODE_S, SDL_KMOD_LALT);
    sc_pending_keys_mark_action(&pks, SC_SCANCODE_S, AKEYCODE_APP_SWITCH);

    struct sc_pending_key key;
    assert(sc_pending_keys_take(&pks, SC_SCANCODE_S, &key));

    // no field may survive in the freed slot
    assert(pks.keys[0].scancode == SC_SCANCODE_UNKNOWN);
    assert(pks.keys[0].action == 0);
    assert(!pks.keys[0].action_sent);
    assert(!pks.keys[0].forwarded);
}

// A free slot is marked by SC_SCANCODE_UNKNOWN; looking up that scancode must
// not match a free slot and hand back a stale action.
static void test_unknown_scancode(void) {
    struct sc_pending_keys pks;
    sc_pending_keys_init(&pks);

    sc_pending_keys_put(&pks, SC_SCANCODE_S, SDL_KMOD_LALT);
    sc_pending_keys_mark_action(&pks, SC_SCANCODE_S, AKEYCODE_APP_SWITCH);

    struct sc_pending_key key;
    assert(sc_pending_keys_take(&pks, SC_SCANCODE_S, &key));

    // the slot is now free but used to hold APP_SWITCH
    assert(!sc_pending_keys_take(&pks, SC_SCANCODE_UNKNOWN, &key));
    assert(!sc_pending_keys_take(&pks, SC_SCANCODE_UNKNOWN, &key));
    assert(!sc_pending_keys_get(&pks, SC_SCANCODE_UNKNOWN));

    // a key-down with an unknown scancode is not remembered at all
    sc_pending_keys_put(&pks, SC_SCANCODE_UNKNOWN, SDL_KMOD_LALT);
    sc_pending_keys_mark_action(&pks, SC_SCANCODE_UNKNOWN,
                                AKEYCODE_APP_SWITCH);
    assert(!sc_pending_keys_take(&pks, SC_SCANCODE_UNKNOWN, &key));
    assert(pks.keys[0].scancode == SC_SCANCODE_UNKNOWN);
    assert(pks.keys[0].action == 0);
}

static void test_full_table(void) {
    struct sc_pending_keys pks;
    sc_pending_keys_init(&pks);

    // fill every slot with a distinct scancode
    for (unsigned i = 0; i < SC_PENDING_KEY_COUNT; ++i) {
        sc_pending_keys_put(&pks, (enum sc_scancode) (i + 1), 0);
    }

    // an extra key is ignored rather than evicting a held chord
    enum sc_scancode extra = (enum sc_scancode) (SC_PENDING_KEY_COUNT + 1);
    sc_pending_keys_put(&pks, extra, 0);
    struct sc_pending_key key;
    assert(!sc_pending_keys_take(&pks, extra, &key));

    // every stored entry is still there
    for (unsigned i = 0; i < SC_PENDING_KEY_COUNT; ++i) {
        assert(sc_pending_keys_take(&pks, (enum sc_scancode) (i + 1), &key));
        assert(key.scancode == (enum sc_scancode) (i + 1));
    }
}

static void test_put_twice(void) {
    struct sc_pending_keys pks;
    sc_pending_keys_init(&pks);

    sc_pending_keys_put(&pks, SC_SCANCODE_S, SDL_KMOD_LALT);
    sc_pending_keys_mark_action(&pks, SC_SCANCODE_S, AKEYCODE_APP_SWITCH);
    sc_pending_keys_mark_forwarded(&pks, SC_SCANCODE_S);

    // pressing the same key again resets its state, and does not use a second
    // slot
    sc_pending_keys_put(&pks, SC_SCANCODE_S, 0);

    struct sc_pending_key key;
    assert(sc_pending_keys_take(&pks, SC_SCANCODE_S, &key));
    assert(key.mod == 0);
    assert(key.action == 0);
    assert(!key.action_sent);
    assert(!key.forwarded);

    for (unsigned i = 1; i < SC_PENDING_KEY_COUNT; ++i) {
        assert(pks.keys[i].scancode == SC_SCANCODE_UNKNOWN);
    }
}

static void test_mark_unknown(void) {
    struct sc_pending_keys pks;
    sc_pending_keys_init(&pks);

    // marking a key that is not down is a no-op, and must not write to a free
    // slot
    sc_pending_keys_mark_action(&pks, SC_SCANCODE_S, AKEYCODE_APP_SWITCH);
    sc_pending_keys_mark_forwarded(&pks, SC_SCANCODE_S);
    assert(pks.keys[0].scancode == SC_SCANCODE_UNKNOWN);
    assert(pks.keys[0].action == 0);
    assert(!pks.keys[0].action_sent);
    assert(!pks.keys[0].forwarded);
}

int main(void) {
    test_put_take();
    test_forwarded();
    test_take_clears_slot();
    test_unknown_scancode();
    test_full_table();
    test_put_twice();
    test_mark_unknown();
    return 0;
}
