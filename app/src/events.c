#include "events.h"

#include "util/log.h"

bool
sc_push_event_impl(uint32_t type, const char *name) {
    SDL_Event event;
    event.type = type;
    int ret = SDL_PushEvent(&event);
    // ret < 0: error (queue full)
    // ret == 0: event was filtered
    // ret == 1: success
    if (ret != 1) {
        LOGE("Could not post %s event: %s", name, SDL_GetError());
        return false;
    }

    return true;
}
