#ifndef CONTROL_H
#define CONTROL_H

#include "control_event.h"

#include <stdbool.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>

#include "net.h"

struct controller {
    socket_t video_socket;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    SDL_cond *event_cond;
    bool stopped;
    struct control_event_queue queue;
};

bool
controller_init(struct controller *controller, socket_t video_socket);

void
controller_destroy(struct controller *controller);

bool
controller_start(struct controller *controller);

void
controller_stop(struct controller *controller);

void
controller_join(struct controller *controller);

// expose simple API to hide control_event_queue
bool
controller_push_event(struct controller *controller,
                      const struct control_event *event);

#endif
