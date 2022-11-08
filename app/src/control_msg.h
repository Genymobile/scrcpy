#ifndef SC_CONTROLMSG_H
#define SC_CONTROLMSG_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "android/input.h"
#include "android/keycodes.h"
#include "coords.h"

#define SC_CONTROL_MSG_MAX_SIZE (1 << 18) // 256k

#define SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH 300
// type: 1 byte; sequence: 8 bytes; paste flag: 1 byte; length: 4 bytes
#define SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH (SC_CONTROL_MSG_MAX_SIZE - 14)

#define POINTER_ID_MOUSE UINT64_C(-1)
#define POINTER_ID_GENERIC_FINGER UINT64_C(-2)

// Used for injecting an additional virtual pointer for pinch-to-zoom
#define POINTER_ID_VIRTUAL_MOUSE UINT64_C(-3)
#define POINTER_ID_VIRTUAL_FINGER UINT64_C(-4)

enum sc_control_msg_type {
    SC_CONTROL_MSG_TYPE_INJECT_KEYCODE,
    SC_CONTROL_MSG_TYPE_INJECT_TEXT,
    SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
    SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
    SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON,
    SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL,
    SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS,
    SC_CONTROL_MSG_TYPE_GET_CLIPBOARD,
    SC_CONTROL_MSG_TYPE_SET_CLIPBOARD,
    SC_CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE,
    SC_CONTROL_MSG_TYPE_ROTATE_DEVICE,
};

enum sc_screen_power_mode {
    // see <https://android.googlesource.com/platform/frameworks/base.git/+/pie-release-2/core/java/android/view/SurfaceControl.java#305>
    SC_SCREEN_POWER_MODE_OFF = 0,
    SC_SCREEN_POWER_MODE_NORMAL = 2,
};

enum sc_copy_key {
    SC_COPY_KEY_NONE,
    SC_COPY_KEY_COPY,
    SC_COPY_KEY_CUT,
};

struct sc_control_msg {
    enum sc_control_msg_type type;
    union {
        struct {
            enum android_keyevent_action action;
            enum android_keycode keycode;
            uint32_t repeat;
            enum android_metastate metastate;
        } inject_keycode;
        struct {
            char *text; // owned, to be freed by free()
        } inject_text;
        struct {
            enum android_motionevent_action action;
            enum android_motionevent_buttons buttons;
            uint64_t pointer_id;
            struct sc_position position;
            float pressure;
        } inject_touch_event;
        struct {
            struct sc_position position;
            float hscroll;
            float vscroll;
            enum android_motionevent_buttons buttons;
        } inject_scroll_event;
        struct {
            enum android_keyevent_action action; // action for the BACK key
            // screen may only be turned on on ACTION_DOWN
        } back_or_screen_on;
        struct {
            enum sc_copy_key copy_key;
        } get_clipboard;
        struct {
            uint64_t sequence;
            char *text; // owned, to be freed by free()
            bool paste;
        } set_clipboard;
        struct {
            enum sc_screen_power_mode mode;
        } set_screen_power_mode;
    };
};

// buf size must be at least CONTROL_MSG_MAX_SIZE
// return the number of bytes written
size_t
sc_control_msg_serialize(const struct sc_control_msg *msg, unsigned char *buf);

void
sc_control_msg_log(const struct sc_control_msg *msg);

void
sc_control_msg_destroy(struct sc_control_msg *msg);

#endif
