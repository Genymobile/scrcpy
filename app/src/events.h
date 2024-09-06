#ifndef SC_EVENTS_H
#define SC_EVENTS_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL_events.h>

enum {
    SC_EVENT_NEW_FRAME = SDL_USEREVENT,
    SC_EVENT_DEVICE_DISCONNECTED,
    SC_EVENT_SERVER_CONNECTION_FAILED,
    SC_EVENT_SERVER_CONNECTED,
    SC_EVENT_USB_DEVICE_DISCONNECTED,
    SC_EVENT_DEMUXER_ERROR,
    SC_EVENT_RECORDER_ERROR,
    SC_EVENT_SCREEN_INIT_SIZE,
    SC_EVENT_TIME_LIMIT_REACHED,
    SC_EVENT_CONTROLLER_ERROR,
};

bool
sc_push_event_impl(uint32_t type, const char *name);

#define sc_push_event(TYPE) sc_push_event_impl(TYPE, # TYPE)

#endif
