#include "control_msg.h"

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
control_msg_serialize(const struct control_msg *msg, unsigned char *buf) {
    buf[0] = msg->type;
    switch (msg->type) {
        case CONTROL_MSG_TYPE_INJECT_KEYCODE:
            buf[1] = msg->inject_keycode.action;
            buffer_write32be(&buf[2], msg->inject_keycode.keycode);
            buffer_write32be(&buf[6], msg->inject_keycode.metastate);
            return 10;
        case CONTROL_MSG_TYPE_INJECT_TEXT: {
            size_t len = write_string(msg->inject_text.text,
                                      CONTROL_MSG_TEXT_MAX_LENGTH, &buf[1]);
            return 1 + len;
        }
        case CONTROL_MSG_TYPE_INJECT_MOUSE_EVENT:
            buf[1] = msg->inject_mouse_event.action;
            buffer_write32be(&buf[2], msg->inject_mouse_event.buttons);
            write_position(&buf[6], &msg->inject_mouse_event.position);
            return 18;
        case CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT:
            write_position(&buf[1], &msg->inject_scroll_event.position);
            buffer_write32be(&buf[13],
                             (uint32_t) msg->inject_scroll_event.hscroll);
            buffer_write32be(&buf[17],
                             (uint32_t) msg->inject_scroll_event.vscroll);
            return 21;
        case CONTROL_MSG_TYPE_SET_CLIPBOARD: {
            size_t len = write_string(msg->inject_text.text,
                                      CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH,
                                      &buf[1]);
            return 1 + len;
        }
        case CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE:
            buf[1] = msg->set_screen_power_mode.mode;
            return 2;
        case CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON:
        case CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL:
        case CONTROL_MSG_TYPE_COLLAPSE_NOTIFICATION_PANEL:
        case CONTROL_MSG_TYPE_GET_CLIPBOARD:
            // no additional data
            return 1;
        default:
            LOGW("Unknown message type: %u", (unsigned) msg->type);
            return 0;
    }
}

void
control_msg_destroy(struct control_msg *msg) {
    switch (msg->type) {
        case CONTROL_MSG_TYPE_INJECT_TEXT:
            SDL_free(msg->inject_text.text);
            break;
        case CONTROL_MSG_TYPE_SET_CLIPBOARD:
            SDL_free(msg->set_clipboard.text);
            break;
        default:
            // do nothing
            break;
    }
}
