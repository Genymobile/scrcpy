#include "keyboard_uhid.h"

#include "util/log.h"

/** Downcast key processor to keyboard_uhid */
#define DOWNCAST(KP) container_of(KP, struct sc_keyboard_uhid, key_processor)

#define UHID_KEYBOARD_ID 1

static void
sc_key_processor_process_key(struct sc_key_processor *kp,
                             const struct sc_key_event *event,
                             uint64_t ack_to_wait) {
    (void) ack_to_wait;

    if (event->repeat) {
        // In USB HID protocol, key repeat is handled by the host (Android), so
        // just ignore key repeat here.
        return;
    }

    struct sc_keyboard_uhid *kb = DOWNCAST(kp);

    struct sc_hid_event hid_event;

    // Not all keys are supported, just ignore unsupported keys
    if (sc_hid_keyboard_event_from_key(&kb->hid, &hid_event, event)) {
        struct sc_control_msg msg;
        msg.type = SC_CONTROL_MSG_TYPE_UHID_INPUT;
        msg.uhid_input.id = UHID_KEYBOARD_ID;

        assert(hid_event.size <= SC_HID_MAX_SIZE);
        memcpy(msg.uhid_input.data, hid_event.data, hid_event.size);
        msg.uhid_input.size = hid_event.size;

        if (!sc_controller_push_msg(kb->controller, &msg)) {
            LOGE("Could not send UHID_INPUT message (key)");
        }
    }
}

bool
sc_keyboard_uhid_init(struct sc_keyboard_uhid *kb,
                      struct sc_controller *controller) {
    sc_hid_keyboard_init(&kb->hid);

    kb->controller = controller;

    static const struct sc_key_processor_ops ops = {
        .process_key = sc_key_processor_process_key,
        // Never forward text input via HID (all the keys are injected
        // separately)
        .process_text = NULL,
    };

    // Clipboard synchronization is requested over the same control socket, so
    // there is no need for a specific synchronization mechanism
    kb->key_processor.async_paste = false;
    kb->key_processor.ops = &ops;

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_UHID_CREATE;
    msg.uhid_create.id = UHID_KEYBOARD_ID;
    msg.uhid_create.report_desc = SC_HID_KEYBOARD_REPORT_DESC;
    msg.uhid_create.report_desc_size = SC_HID_KEYBOARD_REPORT_DESC_LEN;
    if (!sc_controller_push_msg(controller, &msg)) {
        LOGE("Could not send UHID_CREATE message (keyboard)");
        return false;
    }

    return true;
}
