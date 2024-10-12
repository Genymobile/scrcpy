#ifndef SC_EVENTS_H
#define SC_EVENTS_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_events.h>

enum {
    SC_EVENT_NEW_FRAME = SDL_USEREVENT,
    SC_EVENT_RUN_ON_MAIN_THREAD,
    SC_EVENT_DEVICE_DISCONNECTED,
    SC_EVENT_SERVER_CONNECTION_FAILED,
    SC_EVENT_SERVER_CONNECTED,
    SC_EVENT_USB_DEVICE_DISCONNECTED,
    SC_EVENT_DEMUXER_ERROR,
    SC_EVENT_RECORDER_ERROR,
    SC_EVENT_SCREEN_INIT_SIZE,
    SC_EVENT_TIME_LIMIT_REACHED,
    SC_EVENT_CONTROLLER_ERROR,
    SC_EVENT_AOA_OPEN_ERROR,
};

bool
sc_push_event_impl(uint32_t type, const char *name);

#define sc_push_event(TYPE) sc_push_event_impl(TYPE, # TYPE)

typedef void (*sc_runnable_fn)(void *userdata);

bool
sc_post_to_main_thread(sc_runnable_fn run, void *userdata);

void
sc_reject_new_runnables(void);

#endif
