#include "events.h"

#include <assert.h>

#include "util/log.h"
#include "util/thread.h"

bool
sc_push_event_impl(uint32_t type, void* ptr, const char *name) {
    SDL_Event event = {
        .user = {
            .type = type,
            .data1 = ptr,
        }
    };
    bool ok = SDL_PushEvent(&event);
    if (!ok) {
        LOGE("Could not post %s event: %s", name, SDL_GetError());
        return false;
    }

    return true;
}

bool
sc_post_to_main_thread(sc_runnable_fn run, void *userdata) {
    SDL_Event event = {
        .user = {
            .type = SC_EVENT_RUN_ON_MAIN_THREAD,
            .data1 = run,
            .data2 = userdata,
        },
    };
    bool ok = SDL_PushEvent(&event);
    if (!ok) {
        LOGW("Could not post runnable to main thread: %s", SDL_GetError());
        return false;
    }

    return true;
}

static bool SDLCALL
task_event_filter(void *userdata, SDL_Event *event) {
    (void) userdata;

    if (event->type == SC_EVENT_RUN_ON_MAIN_THREAD) {
        // Reject this event type from now on
        return false;
    }

    return true;
}

void
sc_reject_new_runnables(void) {
    assert(sc_thread_get_id() == SC_MAIN_THREAD_ID);

    SDL_SetEventFilter(task_event_filter, NULL);
}
