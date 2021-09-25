#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "aoa_hid.h"

/**
 * Because of dual-report, when we send hid events, we need to add report id
 * as prefix, so keyboard keys looks like
 * `0x01 Modifier Reserved Key Key Key Key Key Key` and media keys looks like
 * `0x02 MediaMask` (one key per bit for media keys).
 */
#define HID_REPORT_ID_INDEX 0
#define HID_KEYBOARD_MODIFIER_INDEX (HID_REPORT_ID_INDEX + 1)
#define HID_KEYBOARD_MODIFIER_NONE 0x00
#define HID_KEYBOARD_MODIFIER_LEFT_CONTROL (1 << 0)
#define HID_KEYBOARD_MODIFIER_LEFT_SHIFT (1 << 1)
#define HID_KEYBOARD_MODIFIER_LEFT_ALT (1 << 2)
#define HID_KEYBOARD_MODIFIER_LEFT_GUI (1 << 3)
#define HID_KEYBOARD_MODIFIER_RIGHT_CONTROL (1 << 4)
#define HID_KEYBOARD_MODIFIER_RIGHT_SHIFT (1 << 5)
#define HID_KEYBOARD_MODIFIER_RIGHT_ALT (1 << 6)
#define HID_KEYBOARD_MODIFIER_RIGHT_GUI (1 << 7)
// USB HID protocol says 6 keys in an event is the requirement for BIOS
// keyboard support, though OS could support more keys via modifying the report
// desc, I think 6 is enough for us.
#define HID_KEYBOARD_MAX_KEYS 6
#define HID_KEYBOARD_EVENT_SIZE (3 + HID_KEYBOARD_MAX_KEYS)
#define HID_KEYBOARD_REPORT_ID 0x01
#define HID_KEYBOARD_RESERVED 0x00
#define HID_KEYBOARD_ERROR_ROLL_OVER 0x01

// See "SDL2/SDL_scancode.h".
// Maybe SDL_Keycode is used by most people,
// but SDL_Scancode is taken from USB HID protocol so I perfer this.
// 0x65 is Application, typically AT-101 Keyboard ends here.
#define HID_KEYBOARD_KEYS 0x66

/**
 * HID keyboard events are sequence-based, every time keyboard state changes
 * it sends an array of currently pressed keys, the host is responsible for
 * compare events and determine which key becomes pressed and which key becomes
 * released. In order to convert SDL_KeyboardEvent to HID events, we first use
 * an array of keys to save each keys' state. And when a SDL_KeyboardEvent was
 * emitted, we updated our state, this is done by hid_keyboard_update_state(),
 * and then we use a loop to generate HID events, the sequence of array elements
 * is unimportant and when too much keys pressed at the same time (more than
 * report count), we should generate phantom state. This is done by
 * hid_keyboard_get_hid_event(). Don't forget that modifiers should be updated
 * too, even for phantom state.
 */
struct hid_keyboard {
    struct aoa *aoa;
    uint16_t accessory_id;
    bool keys[HID_KEYBOARD_KEYS];
};

bool
hid_keyboard_init(struct hid_keyboard *kb, struct aoa *aoa);
/**
 * Return false if unsupported keys is received,
 * and be safe to ignore the HID event.
 * In fact we are not only convert events, we also UPDATE internal key states.
 */
bool
hid_keyboard_convert_event(struct hid_keyboard *kb,
    struct hid_event *hid_event, const SDL_KeyboardEvent *event);
void hid_keyboard_destroy(struct hid_keyboard *kb);

#endif
