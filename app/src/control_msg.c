#include "control_msg.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "util/binary.h"
#include "util/log.h"
#include "util/str.h"

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
    "pointer-down",
    "pointer-up",
    "hover-move",
    "scroll",
    "hover-enter",
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

static const char *const copy_key_labels[] = {
    "none",
    "copy",
    "cut",
};

static inline const char *
get_well_known_pointer_id_name(uint64_t pointer_id) {
    switch (pointer_id) {
        case POINTER_ID_MOUSE:
            return "mouse";
        case POINTER_ID_GENERIC_FINGER:
            return "finger";
        case POINTER_ID_VIRTUAL_MOUSE:
            return "vmouse";
        case POINTER_ID_VIRTUAL_FINGER:
            return "vfinger";
        default:
            return NULL;
    }
}

static void
write_position(uint8_t *buf, const struct sc_position *position) {
    sc_write32be(&buf[0], position->point.x);
    sc_write32be(&buf[4], position->point.y);
    sc_write16be(&buf[8], position->screen_size.width);
    sc_write16be(&buf[10], position->screen_size.height);
}

// write length (4 bytes) + string (non null-terminated)
static size_t
write_string(const char *utf8, size_t max_len, unsigned char *buf) {
    size_t len = sc_str_utf8_truncation_index(utf8, max_len);
    sc_write32be(buf, len);
    memcpy(&buf[4], utf8, len);
    return 4 + len;
}

size_t
sc_control_msg_serialize(const struct sc_control_msg *msg, unsigned char *buf) {
    buf[0] = msg->type;
    switch (msg->type) {
        case SC_CONTROL_MSG_TYPE_INJECT_KEYCODE:
            buf[1] = msg->inject_keycode.action;
            sc_write32be(&buf[2], msg->inject_keycode.keycode);
            sc_write32be(&buf[6], msg->inject_keycode.repeat);
            sc_write32be(&buf[10], msg->inject_keycode.metastate);
            return 14;
        case SC_CONTROL_MSG_TYPE_INJECT_TEXT: {
            size_t len =
                write_string(msg->inject_text.text,
                             SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH, &buf[1]);
            return 1 + len;
        }
        case SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT:
            buf[1] = msg->inject_touch_event.action;
            sc_write64be(&buf[2], msg->inject_touch_event.pointer_id);
            write_position(&buf[10], &msg->inject_touch_event.position);
            uint16_t pressure =
                sc_float_to_u16fp(msg->inject_touch_event.pressure);
            sc_write16be(&buf[22], pressure);
            sc_write32be(&buf[24], msg->inject_touch_event.buttons);
            return 28;
        case SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT:
            write_position(&buf[1], &msg->inject_scroll_event.position);
            int16_t hscroll =
                sc_float_to_i16fp(msg->inject_scroll_event.hscroll);
            int16_t vscroll =
                sc_float_to_i16fp(msg->inject_scroll_event.vscroll);
            sc_write16be(&buf[13], (uint16_t) hscroll);
            sc_write16be(&buf[15], (uint16_t) vscroll);
            sc_write32be(&buf[17], msg->inject_scroll_event.buttons);
            return 21;
        case SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON:
            buf[1] = msg->inject_keycode.action;
            return 2;
        case SC_CONTROL_MSG_TYPE_GET_CLIPBOARD:
            buf[1] = msg->get_clipboard.copy_key;
            return 2;
        case SC_CONTROL_MSG_TYPE_SET_CLIPBOARD:
            sc_write64be(&buf[1], msg->set_clipboard.sequence);
            buf[9] = !!msg->set_clipboard.paste;
            size_t len = write_string(msg->set_clipboard.text,
                                      SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH,
                                      &buf[10]);
            return 10 + len;
        case SC_CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE:
            buf[1] = msg->set_screen_power_mode.mode;
            return 2;
        case SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL:
        case SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL:
        case SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS:
        case SC_CONTROL_MSG_TYPE_ROTATE_DEVICE:
            // no additional data
            return 1;
        default:
            LOGW("Unknown message type: %u", (unsigned) msg->type);
            return 0;
    }
}

void
sc_control_msg_log(const struct sc_control_msg *msg) {
#define LOG_CMSG(fmt, ...) LOGV("input: " fmt, ## __VA_ARGS__)
    switch (msg->type) {
        case SC_CONTROL_MSG_TYPE_INJECT_KEYCODE:
            LOG_CMSG("key %-4s code=%d repeat=%" PRIu32 " meta=%06lx",
                     KEYEVENT_ACTION_LABEL(msg->inject_keycode.action),
                     (int) msg->inject_keycode.keycode,
                     msg->inject_keycode.repeat,
                     (long) msg->inject_keycode.metastate);
            break;
        case SC_CONTROL_MSG_TYPE_INJECT_TEXT:
            LOG_CMSG("text \"%s\"", msg->inject_text.text);
            break;
        case SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT: {
            int action = msg->inject_touch_event.action
                       & AMOTION_EVENT_ACTION_MASK;
            uint64_t id = msg->inject_touch_event.pointer_id;
            const char *pointer_name = get_well_known_pointer_id_name(id);
            if (pointer_name) {
                // string pointer id
                LOG_CMSG("touch [id=%s] %-4s position=%" PRIi32 ",%" PRIi32
                             " pressure=%f buttons=%06lx",
                         pointer_name,
                         MOTIONEVENT_ACTION_LABEL(action),
                         msg->inject_touch_event.position.point.x,
                         msg->inject_touch_event.position.point.y,
                         msg->inject_touch_event.pressure,
                         (long) msg->inject_touch_event.buttons);
            } else {
                // numeric pointer id
                LOG_CMSG("touch [id=%" PRIu64_ "] %-4s position=%" PRIi32 ",%"
                             PRIi32 " pressure=%f buttons=%06lx",
                         id,
                         MOTIONEVENT_ACTION_LABEL(action),
                         msg->inject_touch_event.position.point.x,
                         msg->inject_touch_event.position.point.y,
                         msg->inject_touch_event.pressure,
                         (long) msg->inject_touch_event.buttons);
            }
            break;
        }
        case SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT:
            LOG_CMSG("scroll position=%" PRIi32 ",%" PRIi32 " hscroll=%f"
                         " vscroll=%f buttons=%06lx",
                     msg->inject_scroll_event.position.point.x,
                     msg->inject_scroll_event.position.point.y,
                     msg->inject_scroll_event.hscroll,
                     msg->inject_scroll_event.vscroll,
                     (long) msg->inject_scroll_event.buttons);
            break;
        case SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON:
            LOG_CMSG("back-or-screen-on %s",
                     KEYEVENT_ACTION_LABEL(msg->inject_keycode.action));
            break;
        case SC_CONTROL_MSG_TYPE_GET_CLIPBOARD:
            LOG_CMSG("get clipboard copy_key=%s",
                     copy_key_labels[msg->get_clipboard.copy_key]);
            break;
        case SC_CONTROL_MSG_TYPE_SET_CLIPBOARD:
            LOG_CMSG("clipboard %" PRIu64_ " %s \"%s\"",
                     msg->set_clipboard.sequence,
                     msg->set_clipboard.paste ? "paste" : "nopaste",
                     msg->set_clipboard.text);
            break;
        case SC_CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE:
            LOG_CMSG("power mode %s",
                     SCREEN_POWER_MODE_LABEL(msg->set_screen_power_mode.mode));
            break;
        case SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL:
            LOG_CMSG("expand notification panel");
            break;
        case SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL:
            LOG_CMSG("expand settings panel");
            break;
        case SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS:
            LOG_CMSG("collapse panels");
            break;
        case SC_CONTROL_MSG_TYPE_ROTATE_DEVICE:
            LOG_CMSG("rotate device");
            break;
        default:
            LOG_CMSG("unknown type: %u", (unsigned) msg->type);
            break;
    }
}

void
sc_control_msg_destroy(struct sc_control_msg *msg) {
    switch (msg->type) {
        case SC_CONTROL_MSG_TYPE_INJECT_TEXT:
            free(msg->inject_text.text);
            break;
        case SC_CONTROL_MSG_TYPE_SET_CLIPBOARD:
            free(msg->set_clipboard.text);
            break;
        default:
            // do nothing
            break;
    }
}
