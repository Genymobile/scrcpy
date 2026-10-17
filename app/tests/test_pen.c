#include <assert.h>
#include <math.h>
#include "pen.h"

int main(void) {
    struct sc_pen pen = {0};
    struct sc_pen_sample sample;
    // Select the active union payload so optimized builds initialize its fields.
    SDL_Event e = {.paxis = {
        .type = SDL_EVENT_PEN_AXIS, .which = 7,
        .axis = SDL_PEN_AXIS_PRESSURE, .value = .25f,
    }};
    assert(!sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.ptouch = {
        .type = SDL_EVENT_PEN_DOWN, .which = 7, .x = 20, .y = 30,
    }};
    assert(sc_pen_process(&pen, &e, &sample));
    assert(sample.action == 0 && sample.pressure == .25f);
    e.ptouch.which = 8;
    assert(!sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.pmotion = {
        .type = SDL_EVENT_PEN_MOTION, .which = 7, .x = 40, .y = 50,
    }};
    assert(sc_pen_process(&pen, &e, &sample));
    assert(sample.action == 2 && sample.x == 40 && sample.y == 50);
    e = (SDL_Event) {.paxis = {
        .type = SDL_EVENT_PEN_AXIS, .which = 7,
        .axis = SDL_PEN_AXIS_PRESSURE, .value = .75f,
    }};
    assert(sc_pen_process(&pen, &e, &sample));
    assert(sample.pressure == .75f && sample.x == 40);
    e.paxis.value = NAN;
    assert(sc_pen_process(&pen, &e, &sample));
    assert(sample.action == 1 && !pen.down);
    e = (SDL_Event) {.ptouch = {.type = SDL_EVENT_PEN_UP, .which = 7}};
    assert(!sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.ptouch = {
        .type = SDL_EVENT_PEN_DOWN, .which = 7,
        .pen_state = SDL_PEN_INPUT_ERASER_TIP,
    }};
    assert(!sc_pen_process(&pen, &e, &sample));
    e.ptouch.pen_state = 0;
    assert(sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.type = SDL_EVENT_WINDOW_FOCUS_LOST};
    assert(sc_pen_process(&pen, &e, &sample));
    assert(sample.action == 1 && !pen.down);
    e = (SDL_Event) {.ptouch = {.type = SDL_EVENT_PEN_DOWN, .which = 7}};
    assert(sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.pproximity = {
        .type = SDL_EVENT_PEN_PROXIMITY_OUT, .which = 7,
    }};
    assert(sc_pen_process(&pen, &e, &sample));
    assert(sample.action == 1);
    e = (SDL_Event) {.ptouch = {.type = SDL_EVENT_PEN_DOWN, .which = 7}};
    assert(sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.paxis = {
        .type = SDL_EVENT_PEN_AXIS, .which = 7,
        .axis = SDL_PEN_AXIS_PRESSURE, .value = .8f,
    }};
    assert(sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.ptouch = {.type = SDL_EVENT_PEN_UP, .which = 7}};
    assert(sc_pen_process(&pen, &e, &sample));
    e = (SDL_Event) {.ptouch = {.type = SDL_EVENT_PEN_DOWN, .which = 7}};
    assert(sc_pen_process(&pen, &e, &sample));
    assert(sample.pressure == 0); // Never reuse the preceding stroke's pressure.
    e = (SDL_Event) {.motion = {
        .type = SDL_EVENT_MOUSE_MOTION, .which = SDL_PEN_MOUSEID,
    }};
    assert(sc_pen_is_synthetic(&e));
    e.motion.which = 123;
    assert(!sc_pen_is_synthetic(&e));
    e = (SDL_Event) {.tfinger = {
        .type = SDL_EVENT_FINGER_DOWN, .touchID = SDL_PEN_TOUCHID,
    }};
    assert(sc_pen_is_synthetic(&e));
    e.tfinger.touchID = 123;
    assert(!sc_pen_is_synthetic(&e));
    return 0;
}
