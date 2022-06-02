#include "hid_keyboard.h"

#include <assert.h>

#include "input_events.h"
#include "util/log.h"

/** Downcast key processor to hid_keyboard */
#define DOWNCAST(KP) container_of(KP, struct sc_hid_keyboard, key_processor)

#define HID_KEYBOARD_ACCESSORY_ID 1

#define HID_MODIFIER_NONE 0x00
#define HID_MODIFIER_LEFT_CONTROL (1 << 0)
#define HID_MODIFIER_LEFT_SHIFT (1 << 1)
#define HID_MODIFIER_LEFT_ALT (1 << 2)
#define HID_MODIFIER_LEFT_GUI (1 << 3)
#define HID_MODIFIER_RIGHT_CONTROL (1 << 4)
#define HID_MODIFIER_RIGHT_SHIFT (1 << 5)
#define HID_MODIFIER_RIGHT_ALT (1 << 6)
#define HID_MODIFIER_RIGHT_GUI (1 << 7)

#define HID_KEYBOARD_INDEX_MODIFIER 0
#define HID_KEYBOARD_INDEX_KEYS 2

// USB HID protocol says 6 keys in an event is the requirement for BIOS
// keyboard support, though OS could support more keys via modifying the report
// desc. 6 should be enough for scrcpy.
#define HID_KEYBOARD_MAX_KEYS 6
#define HID_KEYBOARD_EVENT_SIZE (2 + HID_KEYBOARD_MAX_KEYS)

#define HID_RESERVED 0x00
#define HID_ERROR_ROLL_OVER 0x01

/**
 * For HID over AOAv2, only report descriptor is needed.
 *
 * The specification is available here:
 * <https://www.usb.org/sites/default/files/hid1_11.pdf>
 *
 * In particular, read:
 *  - 6.2.2 Report Descriptor
 *  - Appendix B.1 Protocol 1 (Keyboard)
 *  - Appendix C: Keyboard Implementation
 *
 * Normally a basic HID keyboard uses 8 bytes:
 *     Modifier Reserved Key Key Key Key Key Key
 *
 * You can dump your device's report descriptor with:
 *
 *     sudo usbhid-dump -m vid:pid -e descriptor
 *
 * (change vid:pid' to your device's vendor ID and product ID).
 */
static const unsigned char keyboard_report_desc[]  = {
    // Usage Page (Generic Desktop)
    0x05, 0x01,
    // Usage (Keyboard)
    0x09, 0x06,

    // Collection (Application)
    0xA1, 0x01,

    // Usage Page (Key Codes)
    0x05, 0x07,
    // Usage Minimum (224)
    0x19, 0xE0,
     // Usage Maximum (231)
    0x29, 0xE7,
    // Logical Minimum (0)
    0x15, 0x00,
    // Logical Maximum (1)
    0x25, 0x01,
    // Report Size (1)
    0x75, 0x01,
    // Report Count (8)
    0x95, 0x08,
    // Input (Data, Variable, Absolute): Modifier byte
    0x81, 0x02,

    // Report Size (8)
    0x75, 0x08,
    // Report Count (1)
    0x95, 0x01,
    // Input (Constant): Reserved byte
    0x81, 0x01,

    // Usage Page (LEDs)
    0x05, 0x08,
    // Usage Minimum (1)
    0x19, 0x01,
    // Usage Maximum (5)
    0x29, 0x05,
    // Report Size (1)
    0x75, 0x01,
    // Report Count (5)
    0x95, 0x05,
    // Output (Data, Variable, Absolute): LED report
    0x91, 0x02,

    // Report Size (3)
    0x75, 0x03,
    // Report Count (1)
    0x95, 0x01,
    // Output (Constant): LED report padding
    0x91, 0x01,

    // Usage Page (Key Codes)
    0x05, 0x07,
    // Usage Minimum (0)
    0x19, 0x00,
    // Usage Maximum (101)
    0x29, SC_HID_KEYBOARD_KEYS - 1,
    // Logical Minimum (0)
    0x15, 0x00,
    // Logical Maximum(101)
    0x25, SC_HID_KEYBOARD_KEYS - 1,
    // Report Size (8)
    0x75, 0x08,
    // Report Count (6)
    0x95, HID_KEYBOARD_MAX_KEYS,
    // Input (Data, Array): Keys
    0x81, 0x00,

    // End Collection
    0xC0
};

/**
 * A keyboard HID event is 8 bytes long:
 *
 *  - byte 0: modifiers (1 flag per modifier key, 8 possible modifier keys)
 *  - byte 1: reserved (always 0)
 *  - bytes 2 to 7: pressed keys (6 at most)
 *
 *                   7 6 5 4 3 2 1 0
 *                  +---------------+
 *         byte 0:  |. . . . . . . .| modifiers
 *                  +---------------+
 *                   ^ ^ ^ ^ ^ ^ ^ ^
 *                   | | | | | | | `- left Ctrl
 *                   | | | | | | `--- left Shift
 *                   | | | | | `----- left Alt
 *                   | | | | `------- left Gui
 *                   | | | `--------- right Ctrl
 *                   | | `----------- right Shift
 *                   | `------------- right Alt
 *                   `--------------- right Gui
 *
 *                  +---------------+
 *         byte 1:  |0 0 0 0 0 0 0 0| reserved
 *                  +---------------+
 *
 *                  +---------------+
 *   bytes 2 to 7:  |. . . . . . . .| scancode of 1st key pressed
 *                  +---------------+
 *                  |. . . . . . . .| scancode of 2nd key pressed
 *                  +---------------+
 *                  |. . . . . . . .| scancode of 3rd key pressed
 *                  +---------------+
 *                  |. . . . . . . .| scancode of 4th key pressed
 *                  +---------------+
 *                  |. . . . . . . .| scancode of 5th key pressed
 *                  +---------------+
 *                  |. . . . . . . .| scancode of 6th key pressed
 *                  +---------------+
 *
 * If there are less than 6 keys pressed, the last items are set to 0.
 * For example, if A and W are pressed:
 *
 *                  +---------------+
 *   bytes 2 to 7:  |0 0 0 0 0 1 0 0| A is pressed (scancode = 4)
 *                  +---------------+
 *                  |0 0 0 1 1 0 1 0| W is pressed (scancode = 26)
 *                  +---------------+
 *                  |0 0 0 0 0 0 0 0| ^
 *                  +---------------+ |  only 2 keys are pressed, the
 *                  |0 0 0 0 0 0 0 0| |  remaining items are set to 0
 *                  +---------------+ |
 *                  |0 0 0 0 0 0 0 0| |
 *                  +---------------+ |
 *                  |0 0 0 0 0 0 0 0| v
 *                  +---------------+
 *
 * Pressing more than 6 keys is not supported. If this happens (typically,
 * never in practice), report a "phantom state":
 *
 *                  +---------------+
 *   bytes 2 to 7:  |0 0 0 0 0 0 0 1| ^
 *                  +---------------+ |
 *                  |0 0 0 0 0 0 0 1| |  more than 6 keys pressed:
 *                  +---------------+ |  the list is filled with a special
 *                  |0 0 0 0 0 0 0 1| |  rollover error code (0x01)
 *                  +---------------+ |
 *                  |0 0 0 0 0 0 0 1| |
 *                  +---------------+ |
 *                  |0 0 0 0 0 0 0 1| |
 *                  +---------------+ |
 *                  |0 0 0 0 0 0 0 1| v
 *                  +---------------+
 */

static unsigned char
sdl_keymod_to_hid_modifiers(uint16_t mod) {
    unsigned char modifiers = HID_MODIFIER_NONE;
    if (mod & SC_MOD_LCTRL) {
        modifiers |= HID_MODIFIER_LEFT_CONTROL;
    }
    if (mod & SC_MOD_LSHIFT) {
        modifiers |= HID_MODIFIER_LEFT_SHIFT;
    }
    if (mod & SC_MOD_LALT) {
        modifiers |= HID_MODIFIER_LEFT_ALT;
    }
    if (mod & SC_MOD_LGUI) {
        modifiers |= HID_MODIFIER_LEFT_GUI;
    }
    if (mod & SC_MOD_RCTRL) {
        modifiers |= HID_MODIFIER_RIGHT_CONTROL;
    }
    if (mod & SC_MOD_RSHIFT) {
        modifiers |= HID_MODIFIER_RIGHT_SHIFT;
    }
    if (mod & SC_MOD_RALT) {
        modifiers |= HID_MODIFIER_RIGHT_ALT;
    }
    if (mod & SC_MOD_RGUI) {
        modifiers |= HID_MODIFIER_RIGHT_GUI;
    }
    return modifiers;
}

static bool
sc_hid_keyboard_event_init(struct sc_hid_event *hid_event) {
    unsigned char *buffer = malloc(HID_KEYBOARD_EVENT_SIZE);
    if (!buffer) {
        LOG_OOM();
        return false;
    }

    buffer[HID_KEYBOARD_INDEX_MODIFIER] = HID_MODIFIER_NONE;
    buffer[1] = HID_RESERVED;
    memset(&buffer[HID_KEYBOARD_INDEX_KEYS], 0, HID_KEYBOARD_MAX_KEYS);

    sc_hid_event_init(hid_event, HID_KEYBOARD_ACCESSORY_ID, buffer,
                      HID_KEYBOARD_EVENT_SIZE);
    return true;
}

static inline bool
scancode_is_modifier(enum sc_scancode scancode) {
    return scancode >= SC_SCANCODE_LCTRL && scancode <= SC_SCANCODE_RGUI;
}

static bool
convert_hid_keyboard_event(struct sc_hid_keyboard *kb,
                           struct sc_hid_event *hid_event,
                           const struct sc_key_event *event) {
    enum sc_scancode scancode = event->scancode;
    assert(scancode >= 0);

    // SDL also generates events when only modifiers are pressed, we cannot
    // ignore them totally, for example press 'a' first then press 'Control',
    // if we ignore 'Control' event, only 'a' is sent.
    if (scancode >= SC_HID_KEYBOARD_KEYS && !scancode_is_modifier(scancode)) {
        // Scancode to ignore
        return false;
    }

    if (!sc_hid_keyboard_event_init(hid_event)) {
        LOGW("Could not initialize HID keyboard event");
        return false;
    }

    unsigned char modifiers = sdl_keymod_to_hid_modifiers(event->mods_state);

    if (scancode < SC_HID_KEYBOARD_KEYS) {
        // Pressed is true and released is false
        kb->keys[scancode] = (event->action == SC_ACTION_DOWN);
        LOGV("keys[%02x] = %s", scancode,
             kb->keys[scancode] ? "true" : "false");
    }

    hid_event->buffer[HID_KEYBOARD_INDEX_MODIFIER] = modifiers;

    unsigned char *keys_buffer = &hid_event->buffer[HID_KEYBOARD_INDEX_KEYS];
    // Re-calculate pressed keys every time
    int keys_pressed_count = 0;
    for (int i = 0; i < SC_HID_KEYBOARD_KEYS; ++i) {
        if (kb->keys[i]) {
            // USB HID protocol says that if keys exceeds report count, a
            // phantom state should be reported
            if (keys_pressed_count >= HID_KEYBOARD_MAX_KEYS) {
                // Phantom state:
                //  - Modifiers
                //  - Reserved
                //  - ErrorRollOver * HID_MAX_KEYS
                memset(keys_buffer, HID_ERROR_ROLL_OVER, HID_KEYBOARD_MAX_KEYS);
                goto end;
            }

            keys_buffer[keys_pressed_count] = i;
            ++keys_pressed_count;
        }
    }

end:
    LOGV("hid keyboard: key %-4s scancode=%02x (%u) mod=%02x",
         event->action == SC_ACTION_DOWN ? "down" : "up", event->scancode,
         event->scancode, modifiers);

    return true;
}


static bool
push_mod_lock_state(struct sc_hid_keyboard *kb, uint16_t mods_state) {
    bool capslock = mods_state & SC_MOD_CAPS;
    bool numlock = mods_state & SC_MOD_NUM;
    if (!capslock && !numlock) {
        // Nothing to do
        return true;
    }

    struct sc_hid_event hid_event;
    if (!sc_hid_keyboard_event_init(&hid_event)) {
        LOGW("Could not initialize HID keyboard event");
        return false;
    }

    unsigned i = 0;
    if (capslock) {
        hid_event.buffer[HID_KEYBOARD_INDEX_KEYS + i] = SC_SCANCODE_CAPSLOCK;
        ++i;
    }
    if (numlock) {
        hid_event.buffer[HID_KEYBOARD_INDEX_KEYS + i] = SC_SCANCODE_NUMLOCK;
        ++i;
    }

    if (!sc_aoa_push_hid_event(kb->aoa, &hid_event)) {
        sc_hid_event_destroy(&hid_event);
        LOGW("Could not request HID event (mod lock state)");
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

    struct sc_hid_keyboard *kb = DOWNCAST(kp);

    struct sc_hid_event hid_event;
    // Not all keys are supported, just ignore unsupported keys
    if (convert_hid_keyboard_event(kb, &hid_event, event)) {
        if (!kb->mod_lock_synchronized) {
            // Inject CAPSLOCK and/or NUMLOCK if necessary to synchronize
            // keyboard state
            if (push_mod_lock_state(kb, event->mods_state)) {
                kb->mod_lock_synchronized = true;
            }
        }

        if (ack_to_wait) {
            // Ctrl+v is pressed, so clipboard synchronization has been
            // requested. Wait until clipboard synchronization is acknowledged
            // by the server, otherwise it could paste the old clipboard
            // content.
            hid_event.ack_to_wait = ack_to_wait;
        }

        if (!sc_aoa_push_hid_event(kb->aoa, &hid_event)) {
            sc_hid_event_destroy(&hid_event);
            LOGW("Could not request HID event (key)");
        }
    }
}

bool
sc_hid_keyboard_init(struct sc_hid_keyboard *kb, struct sc_aoa *aoa) {
    kb->aoa = aoa;

    bool ok = sc_aoa_setup_hid(aoa, HID_KEYBOARD_ACCESSORY_ID,
                               keyboard_report_desc,
                               ARRAY_LEN(keyboard_report_desc));
    if (!ok) {
        LOGW("Register HID keyboard failed");
        return false;
    }

    // Reset all states
    memset(kb->keys, false, SC_HID_KEYBOARD_KEYS);

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
    kb->key_processor.ops = &ops;

    return true;
}

void
sc_hid_keyboard_destroy(struct sc_hid_keyboard *kb) {
    // Unregister HID keyboard so the soft keyboard shows again on Android
    bool ok = sc_aoa_unregister_hid(kb->aoa, HID_KEYBOARD_ACCESSORY_ID);
    if (!ok) {
        LOGW("Could not unregister HID keyboard");
    }
}
