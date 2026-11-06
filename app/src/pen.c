#include "pen.h"

#include <math.h>

bool
sc_pen_is_synthetic(const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            return event->motion.which == SDL_PEN_MOUSEID;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return event->button.which == SDL_PEN_MOUSEID;
        case SDL_EVENT_MOUSE_WHEEL:
            return event->wheel.which == SDL_PEN_MOUSEID;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
            return event->tfinger.touchID == SDL_PEN_TOUCHID;
        default:
            return false;
    }
}

bool
sc_pen_process(struct sc_pen *pen, const SDL_Event *event,
               struct sc_pen_sample *sample) {
    unsigned action;
    switch (event->type) {
        case SDL_EVENT_PEN_DOWN:
            if (pen->down || (event->ptouch.pen_state & SDL_PEN_INPUT_ERASER_TIP)) {
                return false;
            }
            if (pen->which != event->ptouch.which) {
                pen->pressure = 0;
            }
            pen->which = event->ptouch.which;
            pen->x = event->ptouch.x;
            pen->y = event->ptouch.y;
            pen->down = true;
            action = 0;
            break;
        case SDL_EVENT_PEN_UP:
            if (!pen->down || event->ptouch.which != pen->which) {
                return false;
            }
            pen->x = event->ptouch.x;
            pen->y = event->ptouch.y;
            action = 1;
            break;
        case SDL_EVENT_PEN_MOTION:
            if (!pen->down || event->pmotion.which != pen->which) {
                return false;
            }
            pen->x = event->pmotion.x;
            pen->y = event->pmotion.y;
            action = 2;
            break;
        case SDL_EVENT_PEN_AXIS:
            if (event->paxis.axis != SDL_PEN_AXIS_PRESSURE
                    || (pen->down && event->paxis.which != pen->which)) {
                return false;
            }
            if (!pen->down) {
                pen->which = event->paxis.which;
            }
            pen->pressure = event->paxis.value;
            if (!pen->down) {
                return false;
            }
            action = 2;
            break;
        case SDL_EVENT_PEN_PROXIMITY_OUT:
            if (!pen->down || event->pproximity.which != pen->which) {
                return false;
            }
            action = 1;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (!pen->down) {
                return false;
            }
            action = 1;
            break;
        default:
            return false;
    }
    if (!isfinite(pen->x) || !isfinite(pen->y) || !isfinite(pen->pressure)) {
        action = 1;
        pen->pressure = 0;
        // Preserve a finite position for downstream coordinate conversion.
        pen->x = 0;
        pen->y = 0;
    }
    if (action == 1) {
        pen->down = false;
        pen->pressure = 0;
    }
    *sample = (struct sc_pen_sample) {
        .action = action,
        .x = pen->x,
        .y = pen->y,
        .pressure = action == 1 ? 0 : pen->pressure < 0 ? 0
                    : pen->pressure > 1 ? 1 : pen->pressure,
    };
    return true;
}
