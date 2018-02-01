#include "controlevent.h"

#include <SDL2/SDL_log.h>
#include <SDL2/SDL_stdinc.h>

#include "lockutil.h"

static inline void write16(Uint8 *buf, Uint16 value) {
    buf[0] = value >> 8;
    buf[1] = value;
}

static inline void write32(Uint8 *buf, Uint32 value) {
    buf[0] = value >> 24;
    buf[1] = value >> 16;
    buf[2] = value >> 8;
    buf[3] = value;
}

static void write_position(Uint8 *buf, const struct position *position) {
    write16(&buf[0], position->point.x);
    write16(&buf[2], position->point.y);
    write16(&buf[4], position->screen_size.width);
    write16(&buf[6], position->screen_size.height);
}

int control_event_serialize(const struct control_event *event, unsigned char *buf) {
    buf[0] = event->type;
    switch (event->type) {
        case CONTROL_EVENT_TYPE_KEYCODE:
            buf[1] = event->keycode_event.action;
            write32(&buf[2], event->keycode_event.keycode);
            write32(&buf[6], event->keycode_event.metastate);
            return 10;
        case CONTROL_EVENT_TYPE_TEXT: {
            // write length (1 byte) + date (non nul-terminated)
            size_t len = strlen(event->text_event.text);
            if (len > TEXT_MAX_LENGTH) {
                len = TEXT_MAX_LENGTH;
            }
            buf[1] = (Uint8) len;
            memcpy(&buf[2], &event->text_event.text, len);
            return 2 + len;
        }
        case CONTROL_EVENT_TYPE_MOUSE:
            buf[1] = event->mouse_event.action;
            write32(&buf[2], event->mouse_event.buttons);
            write_position(&buf[6], &event->mouse_event.position);
            return 14;
        case CONTROL_EVENT_TYPE_SCROLL:
            write_position(&buf[1], &event->scroll_event.position);
            write32(&buf[9], (Uint32) event->scroll_event.hscroll);
            write32(&buf[13], (Uint32) event->scroll_event.vscroll);
            return 17;
        default:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unknown event type: %u", (unsigned) event->type);
            return 0;
    }
}

SDL_bool control_event_queue_is_empty(const struct control_event_queue *queue) {
    return queue->head == queue->tail;
}

SDL_bool control_event_queue_is_full(const struct control_event_queue *queue) {
    return (queue->head + 1) % CONTROL_EVENT_QUEUE_SIZE == queue->tail;
}

SDL_bool control_event_queue_init(struct control_event_queue *queue) {
    queue->head = 0;
    queue->tail = 0;
    // the current implementation may not fail
    return SDL_TRUE;
}

void control_event_queue_destroy(struct control_event_queue *queue) {
    // nothing to do in the current implementation
}

SDL_bool control_event_queue_push(struct control_event_queue *queue, const struct control_event *event) {
    if (control_event_queue_is_full(queue)) {
        return SDL_FALSE;
    }
    queue->data[queue->head] = *event;
    queue->head = (queue->head + 1) % CONTROL_EVENT_QUEUE_SIZE;
    return SDL_TRUE;
}

SDL_bool control_event_queue_take(struct control_event_queue *queue, struct control_event *event) {
    if (control_event_queue_is_empty(queue)) {
        return SDL_FALSE;
    }
    *event = queue->data[queue->tail];
    queue->tail = (queue->tail + 1) % CONTROL_EVENT_QUEUE_SIZE;
    return SDL_TRUE;
}
