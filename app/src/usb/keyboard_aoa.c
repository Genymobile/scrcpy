#include "keyboard_aoa.h"

#include <assert.h>

#include "input_events.h"
#include "util/log.h"

/** Downcast key processor to keyboard_aoa */
#define DOWNCAST(KP) container_of(KP, struct sc_keyboard_aoa, key_processor)

static bool
push_mod_lock_state(struct sc_keyboard_aoa *kb, uint16_t mods_state) {
    struct sc_hid_input hid_input;
    if (!sc_hid_keyboard_generate_input_from_mods(&hid_input, mods_state)) {
        // Nothing to do
        return true;
    }

    if (!sc_aoa_push_input(kb->aoa, &hid_input)) {
        LOGW("Could not push AOA HID input (mod lock state)");
        return false;
    }

    LOGD("HID keyboard state synchronized");

    return true;
}

static void
sc_key_processor_process_key(struct sc_key_processor *kp,
                             const struct sc_key_event *event,
                             uint64_t ack_to_wait) {
    if (event->repeat) {
        // In USB HID protocol, key repeat is handled by the host (Android), so
        // just ignore key repeat here.
        return;
    }

    struct sc_keyboard_aoa *kb = DOWNCAST(kp);

    struct sc_hid_input hid_input;

    // Not all keys are supported, just ignore unsupported keys
    if (sc_hid_keyboard_generate_input_from_key(&kb->hid, &hid_input, event)) {
        if (!kb->mod_lock_synchronized) {
            // Inject CAPSLOCK and/or NUMLOCK if necessary to synchronize
            // keyboard state
            if (push_mod_lock_state(kb, event->mods_state)) {
                kb->mod_lock_synchronized = true;
            }
        }

        // If ack_to_wait is != SC_SEQUENCE_INVALID, then Ctrl+v is pressed, so
        // clipboard synchronization has been requested. Wait until clipboard
        // synchronization is acknowledged by the server, otherwise it could
        // paste the old clipboard content.

        if (!sc_aoa_push_input_with_ack_to_wait(kb->aoa, &hid_input,
                                                ack_to_wait)) {
            LOGW("Could not push AOA HID input (key)");
        }
    }
}

bool
sc_keyboard_aoa_init(struct sc_keyboard_aoa *kb, struct sc_aoa *aoa) {
    kb->aoa = aoa;

    struct sc_hid_open hid_open;
    sc_hid_keyboard_generate_open(&hid_open);

    bool ok = sc_aoa_push_open(aoa, &hid_open, true);
    if (!ok) {
        LOGW("Could not push AOA HID open (keyboard)");
        return false;
    }

    sc_hid_keyboard_init(&kb->hid);

    kb->mod_lock_synchronized = false;

    static const struct sc_key_processor_ops ops = {
        .process_key = sc_key_processor_process_key,
        // Never forward text input via HID (all the keys are injected
        // separately)
        .process_text = NULL,
    };

    // Clipboard synchronization is requested over the control socket, while HID
    // events are sent over AOA, so it must wait for clipboard synchronization
    // to be acknowledged by the device before injecting Ctrl+v.
    kb->key_processor.async_paste = true;
    kb->key_processor.hid = true;
    kb->key_processor.ops = &ops;

    return true;
}

void
sc_keyboard_aoa_destroy(struct sc_keyboard_aoa *kb) {
    (void) kb;
    // Do nothing, kb->aoa will automatically unregister all devices
}
