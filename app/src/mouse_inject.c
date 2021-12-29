#include "mouse_inject.h"

#include <assert.h>

#include "android/input.h"
#include "control_msg.h"
#include "controller.h"
#include "input_events.h"
#include "util/intmap.h"
#include "util/log.h"

/** Downcast mouse processor to sc_mouse_inject */
#define DOWNCAST(MP) container_of(MP, struct sc_mouse_inject, mouse_processor)

static enum android_motionevent_buttons
convert_mouse_buttons(uint32_t state) {
    enum android_motionevent_buttons buttons = 0;
    if (state & SC_MOUSE_BUTTON_LEFT) {
        buttons |= AMOTION_EVENT_BUTTON_PRIMARY;
    }
    if (state & SC_MOUSE_BUTTON_RIGHT) {
        buttons |= AMOTION_EVENT_BUTTON_SECONDARY;
    }
    if (state & SC_MOUSE_BUTTON_MIDDLE) {
        buttons |= AMOTION_EVENT_BUTTON_TERTIARY;
    }
    if (state & SC_MOUSE_BUTTON_X1) {
        buttons |= AMOTION_EVENT_BUTTON_BACK;
    }
    if (state & SC_MOUSE_BUTTON_X2) {
        buttons |= AMOTION_EVENT_BUTTON_FORWARD;
    }
    return buttons;
}

static bool
convert_mouse_action(enum sc_action from, enum android_motionevent_action *to) {
    static const struct sc_intmap_entry actions[] = {
        {SC_ACTION_DOWN, AMOTION_EVENT_ACTION_DOWN},
        {SC_ACTION_UP,   AMOTION_EVENT_ACTION_UP},
    };

    const struct sc_intmap_entry *entry = SC_INTMAP_FIND_ENTRY(actions, from);
    if (entry) {
        *to = entry->value;
        return true;
    }

    return false;
}

static bool
convert_touch_action(enum sc_touch_action from,
                     enum android_motionevent_action *to) {
    static const struct sc_intmap_entry actions[] = {
        {SC_TOUCH_ACTION_MOVE, AMOTION_EVENT_ACTION_MOVE},
        {SC_TOUCH_ACTION_DOWN, AMOTION_EVENT_ACTION_DOWN},
        {SC_TOUCH_ACTION_UP,   AMOTION_EVENT_ACTION_UP},
    };

    const struct sc_intmap_entry *entry = SC_INTMAP_FIND_ENTRY(actions, from);
    if (entry) {
        *to = entry->value;
        return true;
    }

    return false;
}

static bool
convert_mouse_motion(const struct sc_mouse_motion_event *event,
                     struct control_msg *msg) {
    msg->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    msg->inject_touch_event.action = AMOTION_EVENT_ACTION_MOVE;
    msg->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
    msg->inject_touch_event.position = event->position;
    msg->inject_touch_event.pressure = 1.f;
    msg->inject_touch_event.buttons =
        convert_mouse_buttons(event->buttons_state);

    return true;
}

static bool
convert_touch(const struct sc_touch_event *event,
              struct control_msg *msg) {
    msg->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

    if (!convert_touch_action(event->action, &msg->inject_touch_event.action)) {
        return false;
    }

    msg->inject_touch_event.pointer_id = event->pointer_id;
    msg->inject_touch_event.position = event->position;
    msg->inject_touch_event.pressure = event->pressure;
    msg->inject_touch_event.buttons = 0;

    return true;
}

static bool
convert_mouse_click(const struct sc_mouse_click_event *event,
                    struct control_msg *msg) {
    msg->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

    if (!convert_mouse_action(event->action, &msg->inject_touch_event.action)) {
        return false;
    }

    msg->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
    msg->inject_touch_event.position = event->position;
    msg->inject_touch_event.pressure = event->action == SC_ACTION_DOWN
                                     ? 1.f : 0.f;
    msg->inject_touch_event.buttons =
        convert_mouse_buttons(event->buttons_state);

    return true;
}

static bool
convert_mouse_scroll(const struct sc_mouse_scroll_event *event,
                     struct control_msg *msg) {
    msg->type = CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT;
    msg->inject_scroll_event.position = event->position;
    msg->inject_scroll_event.hscroll = event->hscroll;
    msg->inject_scroll_event.vscroll = event->vscroll;

    return true;
}

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_motion_event *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (!convert_mouse_motion(event, &msg)) {
        return;
    }

    if (!controller_push_msg(mi->controller, &msg)) {
        LOGW("Could not request 'inject mouse motion event'");
    }
}

static void
sc_mouse_processor_process_touch(struct sc_mouse_processor *mp,
                                 const struct sc_touch_event *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (convert_touch(event, &msg)) {
        if (!controller_push_msg(mi->controller, &msg)) {
            LOGW("Could not request 'inject touch event'");
        }
    }
}

static void
sc_mouse_processor_process_mouse_click(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_click_event *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (convert_mouse_click(event, &msg)) {
        if (!controller_push_msg(mi->controller, &msg)) {
            LOGW("Could not request 'inject mouse click event'");
        }
    }
}

static void
sc_mouse_processor_process_mouse_scroll(struct sc_mouse_processor *mp,
                                   const struct sc_mouse_scroll_event *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (convert_mouse_scroll(event, &msg)) {
        if (!controller_push_msg(mi->controller, &msg)) {
            LOGW("Could not request 'inject mouse scroll event'");
        }
    }
}

void
sc_mouse_inject_init(struct sc_mouse_inject *mi,
                     struct controller *controller) {
    mi->controller = controller;

    static const struct sc_mouse_processor_ops ops = {
        .process_mouse_motion = sc_mouse_processor_process_mouse_motion,
        .process_touch = sc_mouse_processor_process_touch,
        .process_mouse_click = sc_mouse_processor_process_mouse_click,
        .process_mouse_scroll = sc_mouse_processor_process_mouse_scroll,
    };

    mi->mouse_processor.ops = &ops;
}
