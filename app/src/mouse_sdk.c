#include "mouse_sdk.h"

#include <assert.h>
#include <stdint.h>

#include "android/input.h"
#include "control_msg.h"
#include "controller.h"
#include "input_events.h"
#include "util/log.h"

/** Downcast mouse processor to sc_mouse_sdk */
#define DOWNCAST(MP) container_of(MP, struct sc_mouse_sdk, mouse_processor)

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

static enum android_motionevent_action
convert_mouse_action(enum sc_action action) {
    if (action == SC_ACTION_DOWN) {
        return AMOTION_EVENT_ACTION_DOWN;
    }
    assert(action == SC_ACTION_UP);
    return AMOTION_EVENT_ACTION_UP;
}

static enum android_motionevent_action
convert_touch_action(enum sc_touch_action action) {
    switch (action) {
        case SC_TOUCH_ACTION_MOVE:
            return AMOTION_EVENT_ACTION_MOVE;
        case SC_TOUCH_ACTION_DOWN:
            return AMOTION_EVENT_ACTION_DOWN;
        default:
            assert(action == SC_TOUCH_ACTION_UP);
            return AMOTION_EVENT_ACTION_UP;
    }
}

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_motion_event *event) {
    struct sc_mouse_sdk *m = DOWNCAST(mp);

    if (!m->mouse_hover && !event->buttons_state) {
        // Do not send motion events when no click is pressed
        return;
    }

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
        .inject_touch_event = {
            .action = event->buttons_state ? AMOTION_EVENT_ACTION_MOVE
                                           : AMOTION_EVENT_ACTION_HOVER_MOVE,
            .pointer_id = event->pointer_id,
            .position = event->position,
            .pressure = 1.f,
            .buttons = convert_mouse_buttons(event->buttons_state),
        },
    };

    if (!sc_controller_push_msg(m->controller, &msg)) {
        LOGW("Could not request 'inject mouse motion event'");
    }
}

static void
sc_mouse_processor_process_mouse_click(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_click_event *event) {
    struct sc_mouse_sdk *m = DOWNCAST(mp);

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
        .inject_touch_event = {
            .action = convert_mouse_action(event->action),
            .pointer_id = event->pointer_id,
            .position = event->position,
            .pressure = event->action == SC_ACTION_DOWN ? 1.f : 0.f,
            .action_button = convert_mouse_buttons(event->button),
            .buttons = convert_mouse_buttons(event->buttons_state),
        },
    };

    if (!sc_controller_push_msg(m->controller, &msg)) {
        LOGW("Could not request 'inject mouse click event'");
    }
}

static void
sc_mouse_processor_process_mouse_scroll(struct sc_mouse_processor *mp,
                                   const struct sc_mouse_scroll_event *event) {
    struct sc_mouse_sdk *m = DOWNCAST(mp);

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
        .inject_scroll_event = {
            .position = event->position,
            .hscroll = event->hscroll,
            .vscroll = event->vscroll,
            .buttons = convert_mouse_buttons(event->buttons_state),
        },
    };

    if (!sc_controller_push_msg(m->controller, &msg)) {
        LOGW("Could not request 'inject mouse scroll event'");
    }
}

static void
sc_mouse_processor_process_touch(struct sc_mouse_processor *mp,
                                 const struct sc_touch_event *event) {
    struct sc_mouse_sdk *m = DOWNCAST(mp);

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
        .inject_touch_event = {
            .action = convert_touch_action(event->action),
            .pointer_id = event->pointer_id,
            .position = event->position,
            .pressure = event->pressure,
            .buttons = 0,
        },
    };

    if (!sc_controller_push_msg(m->controller, &msg)) {
        LOGW("Could not request 'inject touch event'");
    }
}

void
sc_mouse_sdk_init(struct sc_mouse_sdk *m, struct sc_controller *controller,
                  bool mouse_hover) {
    m->controller = controller;
    m->mouse_hover = mouse_hover;

    static const struct sc_mouse_processor_ops ops = {
        .process_mouse_motion = sc_mouse_processor_process_mouse_motion,
        .process_mouse_click = sc_mouse_processor_process_mouse_click,
        .process_mouse_scroll = sc_mouse_processor_process_mouse_scroll,
        .process_touch = sc_mouse_processor_process_touch,
    };

    m->mouse_processor.ops = &ops;

    m->mouse_processor.relative_mode = false;
}
