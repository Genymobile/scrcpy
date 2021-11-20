#include "mouse_inject.h"

#include <assert.h>
#include <SDL2/SDL_events.h>

#include "android/input.h"
#include "control_msg.h"
#include "controller.h"
#include "util/log.h"

/** Downcast mouse processor to sc_mouse_inject */
#define DOWNCAST(MP) container_of(MP, struct sc_mouse_inject, mouse_processor)

static enum android_motionevent_buttons
convert_mouse_buttons(uint32_t state) {
    enum android_motionevent_buttons buttons = 0;
    if (state & SDL_BUTTON_LMASK) {
        buttons |= AMOTION_EVENT_BUTTON_PRIMARY;
    }
    if (state & SDL_BUTTON_RMASK) {
        buttons |= AMOTION_EVENT_BUTTON_SECONDARY;
    }
    if (state & SDL_BUTTON_MMASK) {
        buttons |= AMOTION_EVENT_BUTTON_TERTIARY;
    }
    if (state & SDL_BUTTON_X1MASK) {
        buttons |= AMOTION_EVENT_BUTTON_BACK;
    }
    if (state & SDL_BUTTON_X2MASK) {
        buttons |= AMOTION_EVENT_BUTTON_FORWARD;
    }
    return buttons;
}

#define MAP(FROM, TO) case FROM: *to = TO; return true
#define FAIL default: return false
static bool
convert_mouse_action(SDL_EventType from, enum android_motionevent_action *to) {
    switch (from) {
        MAP(SDL_MOUSEBUTTONDOWN, AMOTION_EVENT_ACTION_DOWN);
        MAP(SDL_MOUSEBUTTONUP,   AMOTION_EVENT_ACTION_UP);
        FAIL;
    }
}

static bool
convert_touch_action(SDL_EventType from, enum android_motionevent_action *to) {
    switch (from) {
        MAP(SDL_FINGERMOTION, AMOTION_EVENT_ACTION_MOVE);
        MAP(SDL_FINGERDOWN,   AMOTION_EVENT_ACTION_DOWN);
        MAP(SDL_FINGERUP,     AMOTION_EVENT_ACTION_UP);
        FAIL;
    }
}

static bool
convert_mouse_motion(const SDL_MouseMotionEvent *from, struct screen *screen,
                     struct control_msg *to) {
    to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    to->inject_touch_event.action = AMOTION_EVENT_ACTION_MOVE;
    to->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
    to->inject_touch_event.position.screen_size = screen->frame_size;
    to->inject_touch_event.position.point =
        screen_convert_window_to_frame_coords(screen, from->x, from->y);
    to->inject_touch_event.pressure = 1.f;
    to->inject_touch_event.buttons = convert_mouse_buttons(from->state);

    return true;
}

static bool
convert_touch(const SDL_TouchFingerEvent *from, struct screen *screen,
              struct control_msg *to) {
    to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

    if (!convert_touch_action(from->type, &to->inject_touch_event.action)) {
        return false;
    }

    to->inject_touch_event.pointer_id = from->fingerId;
    to->inject_touch_event.position.screen_size = screen->frame_size;

    int dw;
    int dh;
    SDL_GL_GetDrawableSize(screen->window, &dw, &dh);

    // SDL touch event coordinates are normalized in the range [0; 1]
    int32_t x = from->x * dw;
    int32_t y = from->y * dh;
    to->inject_touch_event.position.point =
        screen_convert_drawable_to_frame_coords(screen, x, y);

    to->inject_touch_event.pressure = from->pressure;
    to->inject_touch_event.buttons = 0;
    return true;
}

static bool
convert_mouse_button(const SDL_MouseButtonEvent *from, struct screen *screen,
                     struct control_msg *to) {
    to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

    if (!convert_mouse_action(from->type, &to->inject_touch_event.action)) {
        return false;
    }

    to->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
    to->inject_touch_event.position.screen_size = screen->frame_size;
    to->inject_touch_event.position.point =
        screen_convert_window_to_frame_coords(screen, from->x, from->y);
    to->inject_touch_event.pressure =
        from->type == SDL_MOUSEBUTTONDOWN ? 1.f : 0.f;
    to->inject_touch_event.buttons =
        convert_mouse_buttons(SDL_BUTTON(from->button));

    return true;
}

static bool
convert_mouse_wheel(const SDL_MouseWheelEvent *from, struct screen *screen,
                    struct control_msg *to) {

    // mouse_x and mouse_y are expressed in pixels relative to the window
    int mouse_x;
    int mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    struct sc_position position = {
        .screen_size = screen->frame_size,
        .point = screen_convert_window_to_frame_coords(screen,
                                                       mouse_x, mouse_y),
    };

    to->type = CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT;

    to->inject_scroll_event.position = position;
    to->inject_scroll_event.hscroll = from->x;
    to->inject_scroll_event.vscroll = from->y;

    return true;
}

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                        const SDL_MouseMotionEvent *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (!convert_mouse_motion(event, mi->screen, &msg)) {
        return;
    }

    if (!controller_push_msg(mi->controller, &msg)) {
        LOGW("Could not request 'inject mouse motion event'");
    }
}

static void
sc_mouse_processor_process_touch(struct sc_mouse_processor *mp,
                                 const SDL_TouchFingerEvent *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (convert_touch(event, mi->screen, &msg)) {
        if (!controller_push_msg(mi->controller, &msg)) {
            LOGW("Could not request 'inject touch event'");
        }
    }
}

static void
sc_mouse_processor_process_mouse_button(struct sc_mouse_processor *mp,
                                        const SDL_MouseButtonEvent *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (convert_mouse_button(event, mi->screen, &msg)) {
        if (!controller_push_msg(mi->controller, &msg)) {
            LOGW("Could not request 'inject mouse button event'");
        }
    }
}

static void
sc_mouse_processor_process_mouse_wheel(struct sc_mouse_processor *mp,
                                       const SDL_MouseWheelEvent *event) {
    struct sc_mouse_inject *mi = DOWNCAST(mp);

    struct control_msg msg;
    if (convert_mouse_wheel(event, mi->screen, &msg)) {
        if (!controller_push_msg(mi->controller, &msg)) {
            LOGW("Could not request 'inject mouse wheel event'");
        }
    }
}

void
sc_mouse_inject_init(struct sc_mouse_inject *mi, struct controller *controller,
                     struct screen *screen) {
    mi->controller = controller;
    mi->screen = screen;

    static const struct sc_mouse_processor_ops ops = {
        .process_mouse_motion = sc_mouse_processor_process_mouse_motion,
        .process_touch = sc_mouse_processor_process_touch,
        .process_mouse_button = sc_mouse_processor_process_mouse_button,
        .process_mouse_wheel = sc_mouse_processor_process_mouse_wheel,
    };

    mi->mouse_processor.ops = &ops;
}
