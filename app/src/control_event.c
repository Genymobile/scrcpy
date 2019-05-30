#include "control_event.h"

#include <string.h>

#include "buffer_util.h"
#include "log.h"
#include "str_util.h"

static void
write_position(uint8_t *buf, const struct position *position) {
    buffer_write32be(&buf[0], position->point.x);
    buffer_write32be(&buf[4], position->point.y);
    buffer_write16be(&buf[8], position->screen_size.width);
    buffer_write16be(&buf[10], position->screen_size.height);
}

// write length (2 bytes) + string (non nul-terminated)
static size_t
write_string(const char *utf8, size_t max_len, unsigned char *buf) {
    size_t len = utf8_truncation_index(utf8, max_len);
    buffer_write16be(buf, (uint16_t) len);
    memcpy(&buf[2], utf8, len);
    return 2 + len;
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
            size_t len = write_string(event->text_event.text,
                                      CONTROL_EVENT_TEXT_MAX_LENGTH, &buf[1]);
            return 1 + len;
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
        case CONTROL_EVENT_TYPE_BACK_OR_SCREEN_ON:
        case CONTROL_EVENT_TYPE_EXPAND_NOTIFICATION_PANEL:
        case CONTROL_EVENT_TYPE_COLLAPSE_NOTIFICATION_PANEL:
        case CONTROL_EVENT_TYPE_GET_CLIPBOARD:
            // no additional data
            return 1;
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
