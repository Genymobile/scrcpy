#include "events.h"

#include <assert.h>

#include "util/log.h"
#include "util/sdl.h"
#include "util/thread.h"

static sc_mutex mutex;
static bool stopped;
static bool initialized;

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
sc_dequeue_event(uint32_t type, SDL_Event *event) {
    return SDL_PeepEvents(event, 1, SDL_GETEVENT, type, type) == 1;
}

bool
sc_dequeue_event_for_tag(uint32_t type, void *tag, SDL_Event *event) {
    SDL_Event queued[64];
    int count = SDL_PeepEvents(queued, 64, SDL_GETEVENT, type, type);
    if (count <= 0) {
        return false;
    }
    bool found = false;
    for (int i = 0; i < count; ++i) {
        if (!found && queued[i].user.data2 == tag) {
            *event = queued[i];
            found = true;
        } else {
            SDL_PushEvent(&queued[i]);
        }
    }
    return found;
}

bool
sc_main_thread_init(void) {
    if (initialized) {
        stopped = false;
        return true;
    }
    stopped = false;
    initialized = sc_mutex_init(&mutex);
    return initialized;
}

void
sc_main_thread_destroy(void) {
    if (!initialized) {
        return;
    }
    sc_mutex_destroy(&mutex);
    initialized = false;
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
    if (!sc_sdl_is_embedded()) {
        SDL_PumpEvents();
    }
}
