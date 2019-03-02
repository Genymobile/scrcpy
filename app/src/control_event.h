#ifndef CONTROLEVENT_H
#define CONTROLEVENT_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_mutex.h>

#include "android/input.h"
#include "android/keycodes.h"
#include "common.h"

#define CONTROL_EVENT_QUEUE_SIZE 64
#define TEXT_MAX_LENGTH 300
#define SERIALIZED_EVENT_MAX_SIZE 3 + TEXT_MAX_LENGTH

enum control_event_type {
    CONTROL_EVENT_TYPE_KEYCODE,
    CONTROL_EVENT_TYPE_TEXT,
    CONTROL_EVENT_TYPE_MOUSE,
    CONTROL_EVENT_TYPE_SCROLL,
    CONTROL_EVENT_TYPE_COMMAND,
};

enum control_event_command {
    CONTROL_EVENT_COMMAND_BACK_OR_SCREEN_ON,
    CONTROL_EVENT_COMMAND_EXPAND_NOTIFICATION_PANEL,
    CONTROL_EVENT_COMMAND_COLLAPSE_NOTIFICATION_PANEL,
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
            enum control_event_command action;
        } command_event;
    };
};

struct control_event_queue {
    struct control_event data[CONTROL_EVENT_QUEUE_SIZE];
    int head;
    int tail;
};

// buf size must be at least SERIALIZED_EVENT_MAX_SIZE
int
control_event_serialize(const struct control_event *event, unsigned char *buf);

bool
control_event_queue_init(struct control_event_queue *queue);

void
control_event_queue_destroy(struct control_event_queue *queue);

bool
control_event_queue_is_empty(const struct control_event_queue *queue);

bool
control_event_queue_is_full(const struct control_event_queue *queue);

// event is copied, the queue does not use the event after the function returns
bool
control_event_queue_push(struct control_event_queue *queue,
                         const struct control_event *event);

bool
control_event_queue_take(struct control_event_queue *queue,
                         struct control_event *event);

void
control_event_destroy(struct control_event *event);

#endif
