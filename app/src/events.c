#include "events.h"

#include <assert.h>

#include "util/log.h"
#include "util/thread.h"

static sc_mutex mutex;
static bool stopped;

bool
sc_push_event_impl(uint32_t type, void *ptr, const char *name) {
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
sc_main_thread_init(void) {
    stopped = false;
    return sc_mutex_init(&mutex);
}

void
sc_main_thread_destroy(void) {
    sc_mutex_destroy(&mutex);
}

bool
sc_run_on_main_thread(sc_runnable_fn run, void *userdata, bool wait_complete) {
    assert(!sc_thread_is_main());

    sc_mutex_lock(&mutex);
    if (stopped) {
        sc_mutex_unlock(&mutex);
        return false;
    }
    bool ok = SDL_RunOnMainThread(run, userdata, wait_complete);
    sc_mutex_unlock(&mutex);
    if (!ok) {
        LOGW("Could not run on main thread: %s", SDL_GetError());
        return false;
    }

    return true;
}

void
sc_main_thread_stop(void) {
    sc_mutex_lock(&mutex);
    stopped = true;
    sc_mutex_unlock(&mutex);

    // Run all remaining runnables on the main thread
    SDL_PumpEvents();
}
