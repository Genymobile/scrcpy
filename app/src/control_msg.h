#ifndef CONTROLMSG_H
#define CONTROLMSG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "android/input.h"
#include "android/keycodes.h"
#include "common.h"

#define CONTROL_MSG_MAX_SIZE (1 << 18) // 256k

#define CONTROL_MSG_INJECT_TEXT_MAX_LENGTH 300
// type: 1 byte; paste flag: 1 byte; length: 4 bytes
#define CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH (CONTROL_MSG_MAX_SIZE - 6)

#define POINTER_ID_MOUSE UINT64_C(-1);
#define POINTER_ID_VIRTUAL_FINGER UINT64_C(-2);

enum control_msg_type {
    CONTROL_MSG_TYPE_INJECT_KEYCODE,
    CONTROL_MSG_TYPE_INJECT_TEXT,
    CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
    CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
    CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON,
    CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    CONTROL_MSG_TYPE_COLLAPSE_NOTIFICATION_PANEL,
    CONTROL_MSG_TYPE_GET_CLIPBOARD,
    CONTROL_MSG_TYPE_SET_CLIPBOARD,
    CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE,
    CONTROL_MSG_TYPE_ROTATE_DEVICE,
};

enum screen_power_mode {
    // see <https://android.googlesource.com/platform/frameworks/base.git/+/pie-release-2/core/java/android/view/SurfaceControl.java#305>
    SCREEN_POWER_MODE_OFF = 0,
    SCREEN_POWER_MODE_NORMAL = 2,
};

struct control_msg {
    enum control_msg_type type;
    union {
        struct {
            enum android_keyevent_action action;
            enum android_keycode keycode;
            uint32_t repeat;
            enum android_metastate metastate;
        } inject_keycode;
        struct {
            char *text; // owned, to be freed by SDL_free()
        } inject_text;
        struct {
            enum android_motionevent_action action;
            enum android_motionevent_buttons buttons;
            uint64_t pointer_id;
            struct position position;
            float pressure;
        } inject_touch_event;
        struct {
            struct position position;
            int32_t hscroll;
            int32_t vscroll;
        } inject_scroll_event;
        struct {
            char *text; // owned, to be freed by SDL_free()
            bool paste;
        } set_clipboard;
        struct {
            enum screen_power_mode mode;
        } set_screen_power_mode;
    };
};

// buf size must be at least CONTROL_MSG_MAX_SIZE
// return the number of bytes written
size_t
control_msg_serialize(const struct control_msg *msg, unsigned char *buf);

void
control_msg_destroy(struct control_msg *msg);

#endif
