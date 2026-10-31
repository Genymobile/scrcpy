#include "events.h"

#include <assert.h>
#ifdef _WIN32
// not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>
#endif

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
sc_dequeue_event(uint32_t type, SDL_Event *event) {
    return SDL_PeepEvents(event, 1, SDL_GETEVENT, type, type) == 1;
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

#ifdef _WIN32
static BOOL WINAPI
windows_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
        sc_push_event(SDL_EVENT_QUIT);
        return TRUE;
    }
    return FALSE;
}

void
sc_set_windows_ctrl_c_handler(void) {
    bool ok = SetConsoleCtrlHandler(windows_ctrl_handler, TRUE);
    if (!ok) {
        LOGW("Could not set Ctrl+C handler");
    }
}
#endif // _WIN32
