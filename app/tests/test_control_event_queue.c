#include <assert.h>
#include <string.h>

#include "control_event.h"

static void test_control_event_queue_empty(void) {
    struct control_event_queue queue;
    SDL_bool init_ok = control_event_queue_init(&queue);
    assert(init_ok);

    assert(control_event_queue_is_empty(&queue));

    struct control_event dummy_event;
    SDL_bool push_ok = control_event_queue_push(&queue, &dummy_event);
    assert(push_ok);
    assert(!control_event_queue_is_empty(&queue));

    SDL_bool take_ok = control_event_queue_take(&queue, &dummy_event);
    assert(take_ok);
    assert(control_event_queue_is_empty(&queue));

    SDL_bool take_empty_ok = control_event_queue_take(&queue, &dummy_event);
    assert(!take_empty_ok); // the queue is empty

    control_event_queue_destroy(&queue);
}

static void test_control_event_queue_full(void) {
    struct control_event_queue queue;
    SDL_bool init_ok = control_event_queue_init(&queue);
    assert(init_ok);

    assert(!control_event_queue_is_full(&queue));

    struct control_event dummy_event;
    // fill the queue
    while (control_event_queue_push(&queue, &dummy_event));

    SDL_bool take_ok = control_event_queue_take(&queue, &dummy_event);
    assert(take_ok);
    assert(!control_event_queue_is_full(&queue));

    control_event_queue_destroy(&queue);
}

static void test_control_event_queue_push_take(void) {
    struct control_event_queue queue;
    SDL_bool init_ok = control_event_queue_init(&queue);
    assert(init_ok);

    struct control_event event = {
        .type = CONTROL_EVENT_TYPE_KEYCODE,
        .keycode_event = {
            .action = AKEY_EVENT_ACTION_DOWN,
            .keycode = AKEYCODE_ENTER,
            .metastate = AMETA_CTRL_LEFT_ON | AMETA_CTRL_ON,
        },
    };

    SDL_bool push1_ok = control_event_queue_push(&queue, &event);
    assert(push1_ok);

    event = (struct control_event) {
        .type = CONTROL_EVENT_TYPE_TEXT,
        .text_event = {
            .text = "abc",
        },
    };

    SDL_bool push2_ok = control_event_queue_push(&queue, &event);
    assert(push2_ok);

    // overwrite event
    SDL_bool take1_ok = control_event_queue_take(&queue, &event);
    assert(take1_ok);
    assert(event.type == CONTROL_EVENT_TYPE_KEYCODE);
    assert(event.keycode_event.action == AKEY_EVENT_ACTION_DOWN);
    assert(event.keycode_event.keycode == AKEYCODE_ENTER);
    assert(event.keycode_event.metastate == (AMETA_CTRL_LEFT_ON | AMETA_CTRL_ON));

    // overwrite event
    SDL_bool take2_ok = control_event_queue_take(&queue, &event);
    assert(take2_ok);
    assert(event.type == CONTROL_EVENT_TYPE_TEXT);
    assert(!strcmp(event.text_event.text, "abc"));

    control_event_queue_destroy(&queue);
}

int main(void) {
    test_control_event_queue_empty();
    test_control_event_queue_full();
    test_control_event_queue_push_take();
    return 0;
}
