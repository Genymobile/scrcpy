#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>

static bool fail_alloc;
static void *
test_reallocarray(void *ptr, size_t count, size_t size) {
    return fail_alloc ? NULL : reallocarray(ptr, count, size);
}

static void *
test_allocarray(size_t count, size_t size) {
    return fail_alloc ? NULL : malloc(count * size);
}

// Compile the real queue and dispatch loop, replacing only allocation and I/O.
#define reallocarray test_reallocarray
#define sc_allocarray test_allocarray
#include "../src/controller.c"
#include "pen.h"
#undef reallocarray
#undef sc_allocarray

static struct sc_controller controller;
static struct sc_control_msg expected[256];
static size_t expected_count;
static size_t sent;
static unsigned interrupts;

bool net_interrupt(sc_socket socket) {
    (void) socket;
    ++interrupts;
    return true;
}

bool sc_receiver_init(struct sc_receiver *r, sc_socket s,
                      const struct sc_receiver_callbacks *cbs, void *data) {
    (void) r; (void) s; (void) cbs; (void) data;
    return true;
}
void sc_receiver_destroy(struct sc_receiver *r) { (void) r; }
bool sc_receiver_start(struct sc_receiver *r) { (void) r; return true; }
void sc_receiver_join(struct sc_receiver *r) { (void) r; }

ssize_t net_send_all(sc_socket socket, const void *buf, size_t len) {
    (void) socket;
    assert(sent < expected_count);
    uint8_t bytes[SC_CONTROL_MSG_MAX_SIZE];
    size_t n = sc_control_msg_serialize(&expected[sent++], bytes);
    assert(n == len && !memcmp(buf, bytes, len));
    if (sent == expected_count) {
        sc_controller_stop(&controller);
    }
    return len;
}

static void ended(struct sc_controller *c, bool error, void *data) {
    (void) c; (void) data;
    assert(!error);
}

static void init(void) {
    static const struct sc_controller_callbacks cbs = {.on_ended = ended};
    expected_count = sent = 0;
    interrupts = 0;
    fail_alloc = false;
    assert(sc_controller_init(&controller, SC_SOCKET_NONE, &cbs, NULL));
}

static struct sc_control_msg pen_msg(unsigned action, float pressure) {
    return (struct sc_control_msg) {
        .type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
        .inject_touch_event = {
            .pointer_id = SC_POINTER_ID_PEN,
            .action = action,
            .position = {.point = {20, 30}, .screen_size = {100, 200}},
            .pressure = pressure,
        },
    };
}

static void push(struct sc_control_msg msg, bool accepted) {
    assert(sc_controller_push_msg(&controller, &msg) == accepted);
    if (accepted) {
        assert(expected_count < ARRAY_LEN(expected));
        expected[expected_count++] = msg;
    }
}

static void drain(void) {
    assert(expected_count);
    assert(!run_controller(&controller));
    assert(sent == expected_count);
    assert(sc_vecdeque_is_empty(&controller.queue));
    sc_controller_destroy(&controller);
}

static void congestion(void) {
    init();
    struct sc_control_msg ordinary = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE,
    };
    for (unsigned i = 0; i < 60; ++i) {
        push(ordinary, true);
    }
    // At and above the ordinary limit, boundaries must survive in FIFO order.
    push(pen_msg(AMOTION_EVENT_ACTION_DOWN, .2f), true);
    for (unsigned i = 0; i < 10000; ++i) {
        push(pen_msg(AMOTION_EVENT_ACTION_MOVE, .5f), false);
    }
    assert(sc_vecdeque_size(&controller.queue) == 61);
    push(pen_msg(AMOTION_EVENT_ACTION_UP, 0), true);
    push(pen_msg(AMOTION_EVENT_ACTION_DOWN, .3f), true);
    push(pen_msg(AMOTION_EVENT_ACTION_CANCEL, 0), true);
    // Grow beyond the preallocated 64; repeated boundaries are not rate-bounded.
    push(pen_msg(AMOTION_EVENT_ACTION_DOWN, .4f), true);
    push(pen_msg(AMOTION_EVENT_ACTION_UP, 0), true);
    push(ordinary, false);
    for (unsigned action = 0; action <= 3; ++action) {
        struct sc_control_msg mouse = pen_msg(action, .5f);
        mouse.inject_touch_event.pointer_id = SC_POINTER_ID_MOUSE;
        push(mouse, false);
        mouse.inject_touch_event.pointer_id = SC_POINTER_ID_GENERIC_FINGER;
        push(mouse, false);
    }
    push((struct sc_control_msg) {.type = SC_CONTROL_MSG_TYPE_UHID_INPUT}, false);
    push((struct sc_control_msg) {.type = SC_CONTROL_MSG_TYPE_UHID_CREATE}, true);
    push((struct sc_control_msg) {.type = SC_CONTROL_MSG_TYPE_UHID_DESTROY}, true);
    drain();
}

static void allocation_failure(void) {
    init();
    while (sc_vecdeque_size(&controller.queue) < controller.queue.cap) {
        push((struct sc_control_msg) {.type = SC_CONTROL_MSG_TYPE_UHID_DESTROY}, true);
    }
    size_t size = sc_vecdeque_size(&controller.queue);
    fail_alloc = true;
    push(pen_msg(AMOTION_EVENT_ACTION_UP, 0), false);
    assert(sc_vecdeque_size(&controller.queue) == size);
    fail_alloc = false;
    assert(controller.stopped && interrupts == 1);
    push(pen_msg(AMOTION_EVENT_ACTION_UP, 0), false);
    assert(interrupts == 1);
    assert(!run_controller(&controller) && sent == 0);
    sc_controller_destroy(&controller);
}

static void from_event(struct sc_pen *pen, SDL_Event *event, bool emitted) {
    struct sc_pen_sample sample;
    assert(sc_pen_process(pen, event, &sample) == emitted);
    if (emitted) {
        push(pen_msg(sample.action, sample.pressure), true);
    }
}

static void pressure_and_strokes(void) {
    init();
    struct sc_pen pen = {0};
    SDL_Event e = {0};
    e.type = SDL_EVENT_PEN_DOWN;
    e.ptouch.which = 7;
    from_event(&pen, &e, true);
    e.type = SDL_EVENT_PEN_AXIS;
    e.paxis.which = 7;
    e.paxis.axis = SDL_PEN_AXIS_PRESSURE;
    e.paxis.value = .2f;
    from_event(&pen, &e, true);
    e.paxis.value = .8f;
    from_event(&pen, &e, true);
    e.type = SDL_EVENT_PEN_UP;
    e.ptouch.which = 7;
    from_event(&pen, &e, true);
    e.type = SDL_EVENT_PEN_MOTION;
    e.pmotion.which = 7;
    from_event(&pen, &e, false);
    e.type = SDL_EVENT_PEN_AXIS;
    e.paxis.which = 7;
    e.paxis.axis = SDL_PEN_AXIS_PRESSURE;
    e.paxis.value = .5f;
    from_event(&pen, &e, false);
    e.type = SDL_EVENT_PEN_DOWN;
    e.ptouch.which = 7;
    from_event(&pen, &e, true);
    e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    from_event(&pen, &e, true);
    // Accepted pressure-only samples and prior stroke MOVE precede release/new DOWN.
    assert(expected_count == 6);
    assert(expected[1].inject_touch_event.pressure == .2f);
    assert(expected[2].inject_touch_event.pressure == .8f);
    drain();
}

static void boundary_cap(void) {
    init();
    for (unsigned i = 0; i < 128; ++i) {
        push(pen_msg(i % 2 ? AMOTION_EVENT_ACTION_UP : AMOTION_EVENT_ACTION_DOWN, .2f), true);
    }
    push(pen_msg(AMOTION_EVENT_ACTION_CANCEL, 0), false);
    assert(controller.stopped && interrupts == 1);
    assert(sc_vecdeque_size(&controller.queue) == 128);
    push(pen_msg(AMOTION_EVENT_ACTION_DOWN, .2f), false);
    assert(!run_controller(&controller) && sent == 0 && interrupts == 1);
    sc_controller_destroy(&controller);
}

int main(void) {
    boundary_cap();
    congestion();
    allocation_failure();
    pressure_and_strokes();
    return 0;
}
