#include "events.h"

#include <assert.h>

#include "util/log.h"
#include "util/thread.h"

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

bool
sc_post_to_main_thread(sc_runnable_fn run, void *userdata) {
    SDL_Event event = {
        .user = {
            .type = SC_EVENT_RUN_ON_MAIN_THREAD,
            .data1 = run,
            .data2 = userdata,
        },
    };
    int ret = SDL_PushEvent(&event);
    // ret < 0: error (queue full)
    // ret == 0: event was filtered
    // ret == 1: success
    if (ret != 1) {
        if (ret == 0) {
            // if ret == 0, this is expected on exit, log in debug mode
            LOGD("Could not post runnable to main thread (filtered)");
        } else {
            assert(ret < 0);
            LOGW("Could not post runnable to main thread: %s", SDL_GetError());
        }
        return false;
    }

    return true;
}

static int SDLCALL
task_event_filter(void *userdata, SDL_Event *event) {
    (void) userdata;

    if (event->type == SC_EVENT_RUN_ON_MAIN_THREAD) {
        // Reject this event type from now on
        return 0;
    }

    return 1;
}

void
sc_reject_new_runnables(void) {
    assert(sc_thread_get_id() == SC_MAIN_THREAD_ID);

    SDL_SetEventFilter(task_event_filter, NULL);
}
