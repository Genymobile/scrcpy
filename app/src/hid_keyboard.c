#include "util/log.h"
#include "hid_keyboard.h"

/**
 * For HID over AOAv2, we only need report descriptor.
 * Normally a basic HID keyboard uses 8 bytes,
 * `Modifier Reserved Key Key Key Key Key Key`.
 * See Appendix B.1 Protocol 1 (Keyboard) and
 * Appendix C: Keyboard Implementation in
 * <https://www.usb.org/sites/default/files/hid1_11.pdf>.
 * But if we want to support media keys on keyboard,
 * we need to use two reports,
 * report id 1 and usage page key codes for basic keyboard,
 * report id 2 and usage page consumer for media keys.
 * See 8. Report Protocol in
 * <https://www.usb.org/sites/default/files/hid1_11.pdf>.
 * The former byte is item type prefix, we only use short items here.
 * For how to calculate an item, read 6.2.2 Report Descriptor in
 * <https://www.usb.org/sites/default/files/hid1_11.pdf>.
 * For Consumer Page tags, see 15 Consumer Page (0x0C) in
 * <https://usb.org/sites/default/files/hut1_2.pdf>.
 */
/**
 * You can dump your device's report descriptor with
 * `sudo usbhid-dump -m vid:pid -e descriptor`.
 * Change `vid:pid` to your device's vendor ID and product ID.
 */
unsigned char kb_report_desc_buffer[]  = {
    // Usage Page (Generic Desktop)
    0x05, 0x01,
    // Usage (Keyboard)
    0x09, 0x06,

    // Collection (Application)
    0xA1, 0x01,
    // Report ID (1)
    0x85, 0x01,

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
    0x29, HID_KEYBOARD_KEYS - 1,
    // Logical Minimum (0)
    0x15, 0x00,
    // Logical Maximum(101)
    0x25, HID_KEYBOARD_KEYS - 1,
    // Report Size (8)
    0x75, 0x08,
    // Report Count (6)
    0x95, HID_KEYBOARD_MAX_KEYS,
    // Input (Data, Array): Keys
    0x81, 0x00,

    // End Collection
    0xC0,

    // Usage Page (Consumer)
    0x05, 0x0C,
    // Usage (Consumer Control)
    0x09, 0x01,

    // Collection (Application)
    0xA1, 0x01,
    // Report ID (2)
    0x85, 0x02,

    // Usage Page (Consumer)
    0x05, 0x0C,
    // Usage (Scan Next Track)
    0x09, 0xB5,
    // Usage (Scan Previous Track)
    0x09, 0xB6,
    // Usage (Stop)
    0x09, 0xB7,
    // Usage (Eject)
    0x09, 0xB8,
    // Usage (Play/Pause)
    0x09, 0xCD,
    // Usage (Mute)
    0x09, 0xE2,
    // Usage (Volume Increment)
    0x09, 0xE9,
    // Usage (Volume Decrement)
    0x09, 0xEA,
    // Logical Minimum (0)
    0x15, 0x00,
    // Logical Maximum (1)
    0x25, 0x01,
    // Report Size (1)
    0x75, 0x01,
    // Report Count (8)
    0x95, 0x08,
    // Input (Data, Array)
    0x81, 0x02,

    // End Collection
    0xC0
};

static unsigned char *create_hid_keyboard_event(void) {
    unsigned char *buffer = malloc(sizeof(*buffer) * HID_KEYBOARD_EVENT_SIZE);
    if (!buffer) {
        return NULL;
    }
    buffer[0] = HID_KEYBOARD_REPORT_ID;
    buffer[1] = HID_KEYBOARD_MODIFIER_NONE;
    buffer[2] = HID_KEYBOARD_RESERVED;
    memset(buffer + HID_KEYBOARD_MODIFIER_INDEX + 2,
        HID_KEYBOARD_RESERVED, HID_KEYBOARD_MAX_KEYS);
    return buffer;
}

static unsigned char *create_hid_media_event(void) {
    unsigned char *buffer = malloc(sizeof(*buffer) * HID_MEDIA_EVENT_SIZE);
    if (!buffer) {
        return NULL;
    }
    buffer[0] = HID_MEDIA_REPORT_ID;
    buffer[1] = HID_MEDIA_KEY_UNDEFINED;
    return buffer;
}

bool
hid_keyboard_init(struct hid_keyboard *kb, struct aoa *aoa) {
    kb->aoa = aoa;
    kb->accessory_id = aoa_get_new_accessory_id(aoa);

    struct report_desc report_desc = {
        kb->accessory_id,
        kb_report_desc_buffer,
        sizeof(kb_report_desc_buffer) / sizeof(kb_report_desc_buffer[0])
    };

    if (aoa_register_hid(aoa, kb->accessory_id, report_desc.size) < 0) {
        LOGW("Register HID for keyboard failed");
        return false;
    }

    if (aoa_set_hid_report_desc(aoa, &report_desc) < 0) {
        LOGW("Set HID report desc for keyboard failed");
        return false;
    }

    // Reset all states.
    memset(kb->keys, false, HID_KEYBOARD_KEYS);
    return true;
}

inline static unsigned char sdl_keymod_to_hid_modifiers(SDL_Keymod mod) {
    unsigned char modifiers = HID_KEYBOARD_MODIFIER_NONE;
    // Not so cool, but more readable, and does not rely on actual value.
    if (mod & KMOD_LCTRL) {
        modifiers |= HID_KEYBOARD_MODIFIER_LEFT_CONTROL;
    }
    if (mod & KMOD_LSHIFT) {
        modifiers |= HID_KEYBOARD_MODIFIER_LEFT_SHIFT;
    }
    if (mod & KMOD_LALT) {
        modifiers |= HID_KEYBOARD_MODIFIER_LEFT_ALT;
    }
    if (mod & KMOD_LGUI) {
        modifiers |= HID_KEYBOARD_MODIFIER_LEFT_GUI;
    }
    if (mod & KMOD_RCTRL) {
        modifiers |= HID_KEYBOARD_MODIFIER_RIGHT_CONTROL;
    }
    if (mod & KMOD_RSHIFT) {
        modifiers |= HID_KEYBOARD_MODIFIER_RIGHT_SHIFT;
    }
    if (mod & KMOD_RALT) {
        modifiers |= HID_KEYBOARD_MODIFIER_RIGHT_ALT;
    }
    if (mod & KMOD_RGUI) {
        modifiers |= HID_KEYBOARD_MODIFIER_RIGHT_GUI;
    }
    return modifiers;
}

inline static bool
convert_hid_keyboard_event(struct hid_keyboard *kb, struct hid_event *hid_event,
    const SDL_KeyboardEvent *event) {
    hid_event->buffer = create_hid_keyboard_event();
    if (!hid_event->buffer) {
        return false;
    }
    hid_event->size = HID_KEYBOARD_EVENT_SIZE;

    unsigned char modifiers = sdl_keymod_to_hid_modifiers(event->keysym.mod);

    SDL_Scancode scancode = event->keysym.scancode;
    // SDL also generates event when only modifiers are pressed,
    // we cannot ignore them totally, for example press `a` first then
    // press `Control`, if we ignore `Control` event, only `a` is sent.
    if (scancode >= 0 && scancode < HID_KEYBOARD_KEYS) {
        // Pressed is true and released is false.
        kb->keys[scancode] = (event->type == SDL_KEYDOWN);
        LOGV("keys[%02x] = %s", scancode,
            kb->keys[scancode] ? "true" : "false");
    }

    // Re-calculate pressed keys every time.
    int keys_pressed_count = 0;
    for (int i = 0; i < HID_KEYBOARD_KEYS; ++i) {
        // USB HID protocol says that if keys exceeds report count,
        // a phantom state should be report.
        if (keys_pressed_count > HID_KEYBOARD_MAX_KEYS) {
            // Pantom state is made of `ReportID, Modifiers, Reserved, ErrorRollOver, ErrorRollOver, ErrorRollOver, ErrorRollOver, ErrorRollOver, ErrorRollOver`.
            memset(hid_event->buffer + HID_KEYBOARD_MODIFIER_INDEX + 2,
                HID_KEYBOARD_ERROR_ROLL_OVER, HID_KEYBOARD_MAX_KEYS);
            // But the modifiers should be report normally for phantom state.
            hid_event->buffer[HID_KEYBOARD_MODIFIER_INDEX] = modifiers;
            return true;
        }
        if (kb->keys[i]) {
            hid_event->buffer[
                HID_KEYBOARD_MODIFIER_INDEX + 2 + keys_pressed_count] = i;
            ++keys_pressed_count;
        }
    }
    hid_event->buffer[HID_KEYBOARD_MODIFIER_INDEX] = modifiers;

    return true;
}

inline static bool
convert_hid_media_event(struct hid_event *hid_event,
    const SDL_KeyboardEvent *event) {
    // Re-calculate pressed keys every time.
    unsigned char media_key = HID_MEDIA_KEY_UNDEFINED;

    SDL_Scancode scancode = event->keysym.scancode;
    if (scancode == SDL_SCANCODE_AUDIONEXT) {
        media_key |= HID_MEDIA_KEY_NEXT;
    }
    if (scancode == SDL_SCANCODE_AUDIOPREV) {
        media_key |= HID_MEDIA_KEY_PREVIOUS;
    }
    if (scancode == SDL_SCANCODE_AUDIOSTOP) {
        media_key |= HID_MEDIA_KEY_STOP;
    }
    if (scancode == SDL_SCANCODE_EJECT) {
        media_key |= HID_MEDIA_KEY_EJECT;
    }
    if (scancode == SDL_SCANCODE_AUDIOPLAY) {
        media_key |= HID_MEDIA_KEY_PLAY_PAUSE;
    }
    if (scancode == SDL_SCANCODE_AUDIOMUTE) {
        media_key |= HID_MEDIA_KEY_MUTE;
    }
    if (scancode == SDL_SCANCODE_AUDIOSTOP) {
        media_key |= HID_MEDIA_KEY_STOP;
    }
    // SDL has no equivalence for HID_MEDIA_KEY_VOLUME_UP and
    // HID_MEDIA_KEY_VOLUME_DOWN, it does have SDL_SCANCODE_VOLUMEUP and
    // SDL_SCANCODE_VOLUMEDOWN, but they are under Usage Page (0x07),
    // which should be a keyboard event.

    // Not all other keys are Usage Page 0x0C,
    // we return NULL for unsupported keys.
    if (media_key == HID_MEDIA_KEY_UNDEFINED) {
        return false;
    }

    hid_event->buffer = create_hid_media_event();
    if (!hid_event->buffer) {
        return false;
    }
    hid_event->size = HID_MEDIA_EVENT_SIZE;

    hid_event->buffer[HID_MEDIA_KEY_INDEX] = media_key;

    return true;
}

bool
hid_keyboard_convert_event(struct hid_keyboard *kb,
    struct hid_event *hid_event, const SDL_KeyboardEvent *event) {
    LOGV(
        "Type: %s, Repeat: %s, Modifiers: %02x, Key: %02x",
        event->type == SDL_KEYDOWN ? "down" : "up",
        event->repeat != 0 ? "true" : "false",
        sdl_keymod_to_hid_modifiers(event->keysym.mod),
        event->keysym.scancode
    );

    hid_event->from_accessory_id = kb->accessory_id;

    if (event->keysym.scancode >= 0 &&
        event->keysym.scancode <= SDL_SCANCODE_MODE) {
        // Usage Page 0x07 (Keyboard).
        return convert_hid_keyboard_event(kb, hid_event, event);
    } else {
        // Others.
        return convert_hid_media_event(hid_event, event);
    }
}

void hid_keyboard_destroy(struct hid_keyboard *kb) {
    // Unregister HID keyboard so the soft keyboard shows again on Android.
    aoa_unregister_hid(kb->aoa, kb->accessory_id);
}
