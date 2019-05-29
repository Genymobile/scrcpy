#ifndef CONTROL_H
#define CONTROL_H

#include <stdbool.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>

#include "cbuf.h"
#include "control_event.h"
#include "net.h"

struct control_event_queue CBUF(struct control_event, 64);

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

bool
controller_push_event(struct controller *controller,
                      const struct control_event *event);

#endif
