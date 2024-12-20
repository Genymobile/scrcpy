#include "hid_keyboard.h"

#include <assert.h>
#include <string.h>

#include "util/log.h"

#define SC_HID_MOD_NONE 0x00
#define SC_HID_MOD_LEFT_CONTROL (1 << 0)
#define SC_HID_MOD_LEFT_SHIFT (1 << 1)
#define SC_HID_MOD_LEFT_ALT (1 << 2)
#define SC_HID_MOD_LEFT_GUI (1 << 3)
#define SC_HID_MOD_RIGHT_CONTROL (1 << 4)
#define SC_HID_MOD_RIGHT_SHIFT (1 << 5)
#define SC_HID_MOD_RIGHT_ALT (1 << 6)
#define SC_HID_MOD_RIGHT_GUI (1 << 7)

#define SC_HID_KEYBOARD_INDEX_MODS 0
#define SC_HID_KEYBOARD_INDEX_KEYS 2

// USB HID protocol says 6 keys in an event is the requirement for BIOS
// keyboard support, though OS could support more keys via modifying the report
// desc. 6 should be enough for scrcpy.
#define SC_HID_KEYBOARD_MAX_KEYS 6
#define SC_HID_KEYBOARD_INPUT_SIZE \
    (SC_HID_KEYBOARD_INDEX_KEYS + SC_HID_KEYBOARD_MAX_KEYS)

#define SC_HID_RESERVED 0x00
#define SC_HID_ERROR_ROLL_OVER 0x01

/**
 * For HID, only report descriptor is needed.
 *
 * The specification is available here:
 * <https://www.usb.org/document-library/device-class-definition-hid-111>
 *
 * In particular, read:
 *  - §6.2.2 Report Descriptor
 *  - Appendix B.1 Protocol 1 (Keyboard)
 *  - Appendix C: Keyboard Implementation
 *
 * The HID Usage Tables is also useful:
 * <https://www.usb.org/document-library/hid-usage-tables-15>
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
static const uint8_t SC_HID_KEYBOARD_REPORT_DESC[] = {
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
    0x95, SC_HID_KEYBOARD_MAX_KEYS,
    // Input (Data, Array): Keys
    0x81, 0x00,

    // End Collection
    0xC0
};

/**
 * A keyboard HID input report is 8 bytes long:
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

static void
sc_hid_keyboard_input_init(struct sc_hid_input *hid_input) {
    hid_input->hid_id = SC_HID_ID_KEYBOARD;
    hid_input->size = SC_HID_KEYBOARD_INPUT_SIZE;

    uint8_t *data = hid_input->data;

    data[SC_HID_KEYBOARD_INDEX_MODS] = SC_HID_MOD_NONE;
    data[1] = SC_HID_RESERVED;
    memset(&data[SC_HID_KEYBOARD_INDEX_KEYS], 0, SC_HID_KEYBOARD_MAX_KEYS);
}

static uint16_t
sc_hid_mod_from_sdl_keymod(uint16_t mod) {
    uint16_t mods = SC_HID_MOD_NONE;
    if (mod & SC_MOD_LCTRL) {
        mods |= SC_HID_MOD_LEFT_CONTROL;
    }
    if (mod & SC_MOD_LSHIFT) {
        mods |= SC_HID_MOD_LEFT_SHIFT;
    }
    if (mod & SC_MOD_LALT) {
        mods |= SC_HID_MOD_LEFT_ALT;
    }
    if (mod & SC_MOD_LGUI) {
        mods |= SC_HID_MOD_LEFT_GUI;
    }
    if (mod & SC_MOD_RCTRL) {
        mods |= SC_HID_MOD_RIGHT_CONTROL;
    }
    if (mod & SC_MOD_RSHIFT) {
        mods |= SC_HID_MOD_RIGHT_SHIFT;
    }
    if (mod & SC_MOD_RALT) {
        mods |= SC_HID_MOD_RIGHT_ALT;
    }
    if (mod & SC_MOD_RGUI) {
        mods |= SC_HID_MOD_RIGHT_GUI;
    }
    return mods;
}

void
sc_hid_keyboard_init(struct sc_hid_keyboard *hid) {
    memset(hid->keys, false, SC_HID_KEYBOARD_KEYS);
}

static inline bool
scancode_is_modifier(enum sc_scancode scancode) {
    return scancode >= SC_SCANCODE_LCTRL && scancode <= SC_SCANCODE_RGUI;
}

bool
sc_hid_keyboard_generate_input_from_key(struct sc_hid_keyboard *hid,
                                        struct sc_hid_input *hid_input,
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

    sc_hid_keyboard_input_init(hid_input);

    uint16_t mods = sc_hid_mod_from_sdl_keymod(event->mods_state);

    if (scancode < SC_HID_KEYBOARD_KEYS) {
        // Pressed is true and released is false
        hid->keys[scancode] = (event->action == SC_ACTION_DOWN);
        LOGV("keys[%02x] = %s", scancode,
             hid->keys[scancode] ? "true" : "false");
    }

    hid_input->data[SC_HID_KEYBOARD_INDEX_MODS] = mods;

    uint8_t *keys_data = &hid_input->data[SC_HID_KEYBOARD_INDEX_KEYS];
    // Re-calculate pressed keys every time
    int keys_pressed_count = 0;
    for (int i = 0; i < SC_HID_KEYBOARD_KEYS; ++i) {
        if (hid->keys[i]) {
            // USB HID protocol says that if keys exceeds report count, a
            // phantom state should be reported
            if (keys_pressed_count >= SC_HID_KEYBOARD_MAX_KEYS) {
                // Phantom state:
                //  - Modifiers
                //  - Reserved
                //  - ErrorRollOver * HID_MAX_KEYS
                memset(keys_data, SC_HID_ERROR_ROLL_OVER,
                       SC_HID_KEYBOARD_MAX_KEYS);
                goto end;
            }

            keys_data[keys_pressed_count] = i;
            ++keys_pressed_count;
        }
    }

end:
    LOGV("hid keyboard: key %-4s scancode=%02x (%u) mod=%02x",
         event->action == SC_ACTION_DOWN ? "down" : "up", event->scancode,
         event->scancode, mods);

    return true;
}

bool
sc_hid_keyboard_generate_input_from_mods(struct sc_hid_input *hid_input,
                                         uint16_t mods_state) {
    bool capslock = mods_state & SC_MOD_CAPS;
    bool numlock = mods_state & SC_MOD_NUM;
    if (!capslock && !numlock) {
        // Nothing to do
        return false;
    }

    sc_hid_keyboard_input_init(hid_input);

    unsigned i = 0;
    if (capslock) {
        hid_input->data[SC_HID_KEYBOARD_INDEX_KEYS + i] = SC_SCANCODE_CAPSLOCK;
        ++i;
    }
    if (numlock) {
        hid_input->data[SC_HID_KEYBOARD_INDEX_KEYS + i] = SC_SCANCODE_NUMLOCK;
        ++i;
    }

    return true;
}

void sc_hid_keyboard_generate_open(struct sc_hid_open *hid_open) {
    hid_open->hid_id = SC_HID_ID_KEYBOARD;
    hid_open->report_desc = SC_HID_KEYBOARD_REPORT_DESC;
    hid_open->report_desc_size = sizeof(SC_HID_KEYBOARD_REPORT_DESC);
}

void sc_hid_keyboard_generate_close(struct sc_hid_close *hid_close) {
    hid_close->hid_id = SC_HID_ID_KEYBOARD;
}
