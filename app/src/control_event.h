#ifndef CONTROLEVENT_H
#define CONTROLEVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "android/input.h"
#include "android/keycodes.h"
#include "common.h"

#define CONTROL_EVENT_TEXT_MAX_LENGTH 300
#define CONTROL_EVENT_CLIPBOARD_TEXT_MAX_LENGTH 4093
#define CONTROL_EVENT_SERIALIZED_MAX_SIZE \
    (3 + CONTROL_EVENT_CLIPBOARD_TEXT_MAX_LENGTH)

enum control_event_type {
    CONTROL_EVENT_TYPE_KEYCODE,
    CONTROL_EVENT_TYPE_TEXT,
    CONTROL_EVENT_TYPE_MOUSE,
    CONTROL_EVENT_TYPE_SCROLL,
    CONTROL_EVENT_TYPE_BACK_OR_SCREEN_ON,
    CONTROL_EVENT_TYPE_EXPAND_NOTIFICATION_PANEL,
    CONTROL_EVENT_TYPE_COLLAPSE_NOTIFICATION_PANEL,
    CONTROL_EVENT_TYPE_GET_CLIPBOARD,
    CONTROL_EVENT_TYPE_SET_CLIPBOARD,
};

struct control_event {
    enum control_event_type type;
    union {
        struct {
            enum android_keyevent_action action;
            enum android_keycode keycode;
            enum android_metastate metastate;
        } keycode_event;
        struct {
            char *text; // owned, to be freed by SDL_free()
        } text_event;
        struct {
            enum android_motionevent_action action;
            enum android_motionevent_buttons buttons;
            struct position position;
        } mouse_event;
        struct {
            struct position position;
            int32_t hscroll;
            int32_t vscroll;
        } scroll_event;
        struct {
            char *text; // owned, to be freed by SDL_free()
        } set_clipboard_event;
    };
};

// buf size must be at least CONTROL_EVENT_SERIALIZED_MAX_SIZE
// return the number of bytes written
size_t
control_event_serialize(const struct control_event *event, unsigned char *buf);

void
control_event_destroy(struct control_event *event);

#endif
