#include "controller.h"

#include <SDL2/SDL_assert.h>

#include "config.h"
#include "lock_util.h"
#include "log.h"

bool
controller_init(struct controller *controller, socket_t video_socket) {
    if (!control_event_queue_init(&controller->queue)) {
        return false;
    }

    if (!(controller->mutex = SDL_CreateMutex())) {
        return false;
    }

    if (!(controller->event_cond = SDL_CreateCond())) {
        SDL_DestroyMutex(controller->mutex);
        return false;
    }

    controller->video_socket = video_socket;
    controller->stopped = false;

    return true;
}

void
controller_destroy(struct controller *controller) {
    SDL_DestroyCond(controller->event_cond);
    SDL_DestroyMutex(controller->mutex);
    control_event_queue_destroy(&controller->queue);
}

bool
controller_push_event(struct controller *controller,
                      const struct control_event *event) {
    bool res;
    mutex_lock(controller->mutex);
    bool was_empty = control_event_queue_is_empty(&controller->queue);
    res = control_event_queue_push(&controller->queue, event);
    if (was_empty) {
        cond_signal(controller->event_cond);
    }
    mutex_unlock(controller->mutex);
    return res;
}

static bool
process_event(struct controller *controller,
              const struct control_event *event) {
    unsigned char serialized_event[SERIALIZED_EVENT_MAX_SIZE];
    int length = control_event_serialize(event, serialized_event);
    if (!length) {
        return false;
    }
    int w = net_send_all(controller->video_socket, serialized_event, length);
    return w == length;
}

static int
run_controller(void *data) {
    struct controller *controller = data;

    for (;;) {
        mutex_lock(controller->mutex);
        while (!controller->stopped
                && control_event_queue_is_empty(&controller->queue)) {
            cond_wait(controller->event_cond, controller->mutex);
        }
        if (controller->stopped) {
            // stop immediately, do not process further events
            mutex_unlock(controller->mutex);
            break;
        }
        struct control_event event;
        bool non_empty = control_event_queue_take(&controller->queue,
                                                      &event);
        SDL_assert(non_empty);
        mutex_unlock(controller->mutex);

        bool ok = process_event(controller, &event);
        control_event_destroy(&event);
        if (!ok) {
            LOGD("Cannot write event to socket");
            break;
        }
    }
    return 0;
}

bool
controller_start(struct controller *controller) {
    LOGD("Starting controller thread");

    controller->thread = SDL_CreateThread(run_controller, "controller",
                                          controller);
    if (!controller->thread) {
        LOGC("Could not start controller thread");
        return false;
    }

    return true;
}

void
controller_stop(struct controller *controller) {
    mutex_lock(controller->mutex);
    controller->stopped = true;
    cond_signal(controller->event_cond);
    mutex_unlock(controller->mutex);
}

void
controller_join(struct controller *controller) {
    SDL_WaitThread(controller->thread, NULL);
}
