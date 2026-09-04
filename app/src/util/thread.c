#include "thread.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mutex.h>

#include "util/log.h"

sc_thread_id SC_MAIN_THREAD_ID;

bool
sc_thread_create(sc_thread *thread, sc_thread_fn fn, const char *name,
                 void *userdata) {
    // The thread name length is limited on some systems. Never use a name
    // longer than 16 bytes (including the final '\0')
    assert(strlen(name) <= 15);

    SDL_Thread *sdl_thread = SDL_CreateThread(fn, name, userdata);
    if (!sdl_thread) {
        LOG_OOM();
        return false;
    }

    thread->thread = sdl_thread;
    return true;
}

static SDL_ThreadPriority
to_sdl_thread_priority(enum sc_thread_priority priority) {
    switch (priority) {
        case SC_THREAD_PRIORITY_TIME_CRITICAL:
            return SDL_THREAD_PRIORITY_TIME_CRITICAL;
        case SC_THREAD_PRIORITY_HIGH:
            return SDL_THREAD_PRIORITY_HIGH;
        case SC_THREAD_PRIORITY_NORMAL:
            return SDL_THREAD_PRIORITY_NORMAL;
        case SC_THREAD_PRIORITY_LOW:
            return SDL_THREAD_PRIORITY_LOW;
        default:
            assert(!"Unknown thread priority");
            return 0;
    }
}

bool
sc_thread_set_priority(enum sc_thread_priority priority) {
    SDL_ThreadPriority sdl_priority = to_sdl_thread_priority(priority);
    bool ok = SDL_SetCurrentThreadPriority(sdl_priority);
    if (!ok) {
        LOGD("Could not set thread priority: %s", SDL_GetError());
        return false;
    }

    return true;
}

void
sc_thread_join(sc_thread *thread, int *status) {
    SDL_WaitThread(thread->thread, status);
}

bool
sc_mutex_init(sc_mutex *mutex) {
    SDL_Mutex *sdl_mutex = SDL_CreateMutex();
    if (!sdl_mutex) {
        LOG_OOM();
        return false;
    }

    mutex->mutex = sdl_mutex;
#ifndef NDEBUG
    atomic_init(&mutex->locker, 0);
#endif
    return true;
}

void
sc_mutex_destroy(sc_mutex *mutex) {
    SDL_DestroyMutex(mutex->mutex);
}

void
sc_mutex_lock(sc_mutex *mutex) {
    // SDL mutexes are recursive, but we don't want to use recursive mutexes
    assert(!sc_mutex_held(mutex));
    SDL_LockMutex(mutex->mutex);
#ifndef NDEBUG
    atomic_store_explicit(&mutex->locker, sc_thread_get_id(),
                          memory_order_relaxed);
#endif
}

void
sc_mutex_unlock(sc_mutex *mutex) {
    assert(sc_mutex_held(mutex));
#ifndef NDEBUG
    atomic_store_explicit(&mutex->locker, 0, memory_order_relaxed);
#endif
    SDL_UnlockMutex(mutex->mutex);
}

sc_thread_id
sc_thread_get_id(void) {
    return SDL_GetCurrentThreadID();
}

bool
sc_thread_is_main(void) {
    return SDL_IsMainThread();
}

#ifndef NDEBUG
bool
sc_mutex_held(struct sc_mutex *mutex) {
    sc_thread_id locker_id =
        atomic_load_explicit(&mutex->locker, memory_order_relaxed);
    return locker_id == sc_thread_get_id();
}
#endif

bool
sc_cond_init(sc_cond *cond) {
    SDL_Condition *sdl_cond = SDL_CreateCondition();
    if (!sdl_cond) {
        LOG_OOM();
        return false;
    }

    cond->cond = sdl_cond;
    return true;
}

void
sc_cond_destroy(sc_cond *cond) {
    SDL_DestroyCondition(cond->cond);
}

void
sc_cond_wait(sc_cond *cond, sc_mutex *mutex) {
    SDL_WaitCondition(cond->cond, mutex->mutex);
#ifndef NDEBUG
    atomic_store_explicit(&mutex->locker, sc_thread_get_id(),
                          memory_order_relaxed);
#endif
}

bool
sc_cond_timedwait(sc_cond *cond, sc_mutex *mutex, sc_tick deadline) {
    sc_tick now = sc_tick_now();
    if (deadline <= now) {
        return false; // timeout
    }

    // Round up to the next millisecond to guarantee that the deadline is
    // reached when returning due to timeout
    uint32_t ms = SC_TICK_TO_MS(deadline - now + SC_TICK_FROM_MS(1) - 1);
    bool signaled = SDL_WaitConditionTimeout(cond->cond, mutex->mutex, ms);
#ifndef NDEBUG
    atomic_store_explicit(&mutex->locker, sc_thread_get_id(),
                          memory_order_relaxed);
#endif
    // The deadline is reached on timeout
    assert(signaled || sc_tick_now() >= deadline);
    return signaled;
}

void
sc_cond_signal(sc_cond *cond) {
    SDL_SignalCondition(cond->cond);
}

void
sc_cond_broadcast(sc_cond *cond) {
    SDL_BroadcastCondition(cond->cond);
}
