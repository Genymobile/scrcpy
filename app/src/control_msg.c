#include "control_msg.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "util/buffer_util.h"
#include "util/log.h"
#include "util/str_util.h"

/**
 * Map an enum value to a string based on an array, without crashing on an
 * out-of-bounds index.
 */
#define ENUM_TO_LABEL(labels, value) \
    ((size_t) (value) < ARRAY_LEN(labels) ? labels[value] : "???")

#define KEYEVENT_ACTION_LABEL(value) \
    ENUM_TO_LABEL(android_keyevent_action_labels, value)

#define MOTIONEVENT_ACTION_LABEL(value) \
    ENUM_TO_LABEL(android_motionevent_action_labels, value)

#define SCREEN_POWER_MODE_LABEL(value) \
    ENUM_TO_LABEL(screen_power_mode_labels, value)

static const char *const android_keyevent_action_labels[] = {
    "down",
    "up",
    "multi",
};

static const char *const android_motionevent_action_labels[] = {
    "down",
    "up",
    "move",
    "cancel",
    "outside",
    "ponter-down",
    "pointer-up",
    "hover-move",
    "scroll",
    "hover-enter"
    "hover-exit",
    "btn-press",
    "btn-release",
};

static const char *const screen_power_mode_labels[] = {
    "off",
    "doze",
    "normal",
    "doze-suspend",
    "suspend",
};

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
    buffer_write32be(buf, len);
    memcpy(&buf[4], utf8, len);
    return 4 + len;
}

static uint16_t
to_fixed_point_16(float f) {
    assert(f >= 0.0f && f <= 1.0f);
    uint32_t u = f * 0x1p16f; // 2^16
    if (u >= 0xffff) {
        u = 0xffff;
    }
    return (uint16_t) u;
}

size_t
control_msg_serialize(const struct control_msg *msg, unsigned char *buf) {
    buf[0] = msg->type;
    switch (msg->type) {
        case CONTROL_MSG_TYPE_INJECT_KEYCODE:
            buf[1] = msg->inject_keycode.action;
            buffer_write32be(&buf[2], msg->inject_keycode.keycode);
            buffer_write32be(&buf[6], msg->inject_keycode.repeat);
            buffer_write32be(&buf[10], msg->inject_keycode.metastate);
            return 14;
        case CONTROL_MSG_TYPE_INJECT_TEXT: {
            size_t len =
                write_string(msg->inject_text.text,
                             CONTROL_MSG_INJECT_TEXT_MAX_LENGTH, &buf[1]);
            return 1 + len;
        }
        case CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT:
            buf[1] = msg->inject_touch_event.action;
            buffer_write64be(&buf[2], msg->inject_touch_event.pointer_id);
            write_position(&buf[10], &msg->inject_touch_event.position);
            uint16_t pressure =
                to_fixed_point_16(msg->inject_touch_event.pressure);
            buffer_write16be(&buf[22], pressure);
            buffer_write32be(&buf[24], msg->inject_touch_event.buttons);
            return 28;
        case CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT:
            write_position(&buf[1], &msg->inject_scroll_event.position);
            buffer_write32be(&buf[13],
                             (uint32_t) msg->inject_scroll_event.hscroll);
            buffer_write32be(&buf[17],
                             (uint32_t) msg->inject_scroll_event.vscroll);
            return 21;
        case CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON:
            buf[1] = msg->inject_keycode.action;
            return 2;
        case CONTROL_MSG_TYPE_SET_CLIPBOARD: {
            buf[1] = !!msg->set_clipboard.paste;
            size_t len = write_string(msg->set_clipboard.text,
                                      CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH,
                                      &buf[2]);
            return 2 + len;
        }
        case CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE:
            buf[1] = msg->set_screen_power_mode.mode;
            return 2;
        case CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL:
        case CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL:
        case CONTROL_MSG_TYPE_COLLAPSE_PANELS:
        case CONTROL_MSG_TYPE_GET_CLIPBOARD:
        case CONTROL_MSG_TYPE_ROTATE_DEVICE:
            // no additional data
            return 1;
        default:
            LOGW("Unknown message type: %u", (unsigned) msg->type);
            return 0;
    }
}

void
control_msg_log(const struct control_msg *msg) {
#define LOG_CMSG(fmt, ...) LOGV("input: " fmt, ## __VA_ARGS__)
    switch (msg->type) {
        case CONTROL_MSG_TYPE_INJECT_KEYCODE:
            LOG_CMSG("key %-4s code=%d repeat=%" PRIu32 " meta=%06lx",
                     KEYEVENT_ACTION_LABEL(msg->inject_keycode.action),
                     (int) msg->inject_keycode.keycode,
                     msg->inject_keycode.repeat,
                     (long) msg->inject_keycode.metastate);
            break;
        case CONTROL_MSG_TYPE_INJECT_TEXT:
            LOG_CMSG("text \"%s\"", msg->inject_text.text);
            break;
        case CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT: {
            int action = msg->inject_touch_event.action
                       & AMOTION_EVENT_ACTION_MASK;
            uint64_t id = msg->inject_touch_event.pointer_id;
            if (id == POINTER_ID_MOUSE || id == POINTER_ID_VIRTUAL_FINGER) {
                // string pointer id
                LOG_CMSG("touch [id=%s] %-4s position=%" PRIi32 ",%" PRIi32
                             " pressure=%g buttons=%06lx",
                         id == POINTER_ID_MOUSE ? "mouse" : "vfinger",
                         MOTIONEVENT_ACTION_LABEL(action),
                         msg->inject_touch_event.position.point.x,
                         msg->inject_touch_event.position.point.y,
                         msg->inject_touch_event.pressure,
                         (long) msg->inject_touch_event.buttons);
            } else {
                // numeric pointer id
#ifndef __WIN32
# define PRIu64_ PRIu64
#else
# define PRIu64_ "I64u"  // Windows...
#endif
                LOG_CMSG("touch [id=%" PRIu64_ "] %-4s position=%" PRIi32 ",%"
                             PRIi32 " pressure=%g buttons=%06lx",
                         id,
                         MOTIONEVENT_ACTION_LABEL(action),
                         msg->inject_touch_event.position.point.x,
                         msg->inject_touch_event.position.point.y,
                         msg->inject_touch_event.pressure,
                         (long) msg->inject_touch_event.buttons);
            }
            break;
        }
        case CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT:
            LOG_CMSG("scroll position=%" PRIi32 ",%" PRIi32 " hscroll=%" PRIi32
                         " vscroll=%" PRIi32,
                     msg->inject_scroll_event.position.point.x,
                     msg->inject_scroll_event.position.point.y,
                     msg->inject_scroll_event.hscroll,
                     msg->inject_scroll_event.vscroll);
            break;
        case CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON:
            LOG_CMSG("back-or-screen-on %s",
                     KEYEVENT_ACTION_LABEL(msg->inject_keycode.action));
            break;
        case CONTROL_MSG_TYPE_SET_CLIPBOARD:
            LOG_CMSG("clipboard %s \"%s\"",
                     msg->set_clipboard.paste ? "paste" : "copy",
                     msg->set_clipboard.text);
            break;
        case CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE:
            LOG_CMSG("power mode %s",
                     SCREEN_POWER_MODE_LABEL(msg->set_screen_power_mode.mode));
            break;
        case CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL:
            LOG_CMSG("expand notification panel");
            break;
        case CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL:
            LOG_CMSG("expand settings panel");
            break;
        case CONTROL_MSG_TYPE_COLLAPSE_PANELS:
            LOG_CMSG("collapse panels");
            break;
        case CONTROL_MSG_TYPE_GET_CLIPBOARD:
            LOG_CMSG("get clipboard");
            break;
        case CONTROL_MSG_TYPE_ROTATE_DEVICE:
            LOG_CMSG("rotate device");
            break;
        default:
            LOG_CMSG("unknown type: %u", (unsigned) msg->type);
            break;
    }
}

void
control_msg_destroy(struct control_msg *msg) {
    switch (msg->type) {
        case CONTROL_MSG_TYPE_INJECT_TEXT:
            free(msg->inject_text.text);
            break;
        case CONTROL_MSG_TYPE_SET_CLIPBOARD:
            free(msg->set_clipboard.text);
            break;
        default:
            // do nothing
            break;
    }
}
