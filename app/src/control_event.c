#include "control_event.h"

#include <string.h>

#include "buffer_util.h"
#include "log.h"

static void
write_position(uint8_t *buf, const struct position *position) {
    buffer_write32be(&buf[0], position->point.x);
    buffer_write32be(&buf[4], position->point.y);
    buffer_write16be(&buf[8], position->screen_size.width);
    buffer_write16be(&buf[10], position->screen_size.height);
}

size_t
control_event_serialize(const struct control_event *event, unsigned char *buf) {
    buf[0] = event->type;
    switch (event->type) {
        case CONTROL_EVENT_TYPE_KEYCODE:
            buf[1] = event->keycode_event.action;
            buffer_write32be(&buf[2], event->keycode_event.keycode);
            buffer_write32be(&buf[6], event->keycode_event.metastate);
            return 10;
        case CONTROL_EVENT_TYPE_TEXT: {
            // write length (2 bytes) + string (non nul-terminated)
            size_t len = strlen(event->text_event.text);
            if (len > CONTROL_EVENT_TEXT_MAX_LENGTH) {
                // injecting a text takes time, so limit the text length
                len = CONTROL_EVENT_TEXT_MAX_LENGTH;
            }
            buffer_write16be(&buf[1], (uint16_t) len);
            memcpy(&buf[3], event->text_event.text, len);
            return 3 + len;
        }
        case CONTROL_EVENT_TYPE_MOUSE:
            buf[1] = event->mouse_event.action;
            buffer_write32be(&buf[2], event->mouse_event.buttons);
            write_position(&buf[6], &event->mouse_event.position);
            return 18;
        case CONTROL_EVENT_TYPE_SCROLL:
            write_position(&buf[1], &event->scroll_event.position);
            buffer_write32be(&buf[13], (uint32_t) event->scroll_event.hscroll);
            buffer_write32be(&buf[17], (uint32_t) event->scroll_event.vscroll);
            return 21;
        case CONTROL_EVENT_TYPE_COMMAND:
            buf[1] = event->command_event.action;
            return 2;
        default:
            LOGW("Unknown event type: %u", (unsigned) event->type);
            return 0;
    }
}

void
control_event_destroy(struct control_event *event) {
    if (event->type == CONTROL_EVENT_TYPE_TEXT) {
        SDL_free(event->text_event.text);
    }
}
