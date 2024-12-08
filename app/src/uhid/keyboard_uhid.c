#include "keyboard_uhid.h"

#include "util/log.h"

/** Downcast key processor to keyboard_uhid */
#define DOWNCAST(KP) container_of(KP, struct sc_keyboard_uhid, key_processor)

/** Downcast uhid_receiver to keyboard_uhid */
#define DOWNCAST_RECEIVER(UR) \
    container_of(UR, struct sc_keyboard_uhid, uhid_receiver)

static void
sc_keyboard_uhid_send_input(struct sc_keyboard_uhid *kb,
                            const struct sc_hid_input *hid_input) {
    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_UHID_INPUT;
    msg.uhid_input.id = hid_input->hid_id;

    assert(hid_input->size <= SC_HID_MAX_SIZE);
    memcpy(msg.uhid_input.data, hid_input->data, hid_input->size);
    msg.uhid_input.size = hid_input->size;

    if (!sc_controller_push_msg(kb->controller, &msg)) {
        LOGE("Could not push UHID_INPUT message (key)");
    }
}

static void
sc_keyboard_uhid_synchronize_mod(struct sc_keyboard_uhid *kb) {
    SDL_Keymod sdl_mod = SDL_GetModState();
    uint16_t mod = sc_mods_state_from_sdl(sdl_mod) & (SC_MOD_CAPS | SC_MOD_NUM);
    uint16_t diff = mod ^ kb->device_mod;

    if (diff) {
        // Inherently racy (the HID output reports arrive asynchronously in
        // response to key presses), but will re-synchronize on next key press
        // or HID output anyway
        kb->device_mod = mod;

        struct sc_hid_input hid_input;
        if (!sc_hid_keyboard_generate_input_from_mods(&hid_input, diff)) {
            return;
        }

        LOGV("HID keyboard state synchronized");

        sc_keyboard_uhid_send_input(kb, &hid_input);
    }
}

static void
sc_key_processor_process_key(struct sc_key_processor *kp,
                             const struct sc_key_event *event,
                             uint64_t ack_to_wait) {
    (void) ack_to_wait;

    assert(sc_thread_get_id() == SC_MAIN_THREAD_ID);

    if (event->repeat) {
        // In USB HID protocol, key repeat is handled by the host (Android), so
        // just ignore key repeat here.
        return;
    }

    struct sc_keyboard_uhid *kb = DOWNCAST(kp);

    struct sc_hid_input hid_input;

    // Not all keys are supported, just ignore unsupported keys
    if (sc_hid_keyboard_generate_input_from_key(&kb->hid, &hid_input, event)) {
        if (event->scancode == SC_SCANCODE_CAPSLOCK) {
            kb->device_mod ^= SC_MOD_CAPS;
        } else if (event->scancode == SC_SCANCODE_NUMLOCK) {
            kb->device_mod ^= SC_MOD_NUM;
        } else {
            // Synchronize modifiers (only if the scancode itself does not
            // change the modifiers)
            sc_keyboard_uhid_synchronize_mod(kb);
        }
        sc_keyboard_uhid_send_input(kb, &hid_input);
    }
}

static unsigned
sc_keyboard_uhid_to_sc_mod(uint8_t hid_led) {
    // <https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf>
    // (chapter 11: LED page)
    unsigned mod = 0;
    if (hid_led & 0x01) {
        mod |= SC_MOD_NUM;
    }
    if (hid_led & 0x02) {
        mod |= SC_MOD_CAPS;
    }
    return mod;
}

void
sc_keyboard_uhid_process_hid_output(struct sc_keyboard_uhid *kb,
                                    const uint8_t *data, size_t size) {
    assert(sc_thread_get_id() == SC_MAIN_THREAD_ID);

    assert(size);

    // Also check at runtime (do not trust the server)
    if (!size) {
        LOGE("Unexpected empty HID output message");
        return;
    }

    uint8_t hid_led = data[0];
    uint16_t device_mod = sc_keyboard_uhid_to_sc_mod(hid_led);
    kb->device_mod = device_mod;
}

bool
sc_keyboard_uhid_init(struct sc_keyboard_uhid *kb,
                      struct sc_controller *controller) {
    sc_hid_keyboard_init(&kb->hid);

    kb->controller = controller;
    kb->device_mod = 0;

    static const struct sc_key_processor_ops ops = {
        .process_key = sc_key_processor_process_key,
        // Never forward text input via HID (all the keys are injected
        // separately)
        .process_text = NULL,
    };

    // Clipboard synchronization is requested over the same control socket, so
    // there is no need for a specific synchronization mechanism
    kb->key_processor.async_paste = false;
    kb->key_processor.hid = true;
    kb->key_processor.ops = &ops;

    struct sc_hid_open hid_open;
    sc_hid_keyboard_generate_open(&hid_open);
    assert(hid_open.hid_id == SC_HID_ID_KEYBOARD);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_UHID_CREATE;
    msg.uhid_create.id = SC_HID_ID_KEYBOARD;
    msg.uhid_create.vendor_id = 0;
    msg.uhid_create.product_id = 0;
    msg.uhid_create.name = NULL;
    msg.uhid_create.report_desc = hid_open.report_desc;
    msg.uhid_create.report_desc_size = hid_open.report_desc_size;
    if (!sc_controller_push_msg(controller, &msg)) {
        LOGE("Could not send UHID_CREATE message (keyboard)");
        return false;
    }

    return true;
}
