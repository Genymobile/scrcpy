#include "thread.h"

#include <assert.h>
#include <string.h>
#include <SDL2/SDL_thread.h>

#include "log.h"

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

void
sc_thread_join(sc_thread *thread, int *status) {
    SDL_WaitThread(thread->thread, status);
}

bool
sc_mutex_init(sc_mutex *mutex) {
    SDL_mutex *sdl_mutex = SDL_CreateMutex();
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
    int r = SDL_LockMutex(mutex->mutex);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not lock mutex: %s", SDL_GetError());
        abort();
    }

    atomic_store_explicit(&mutex->locker, sc_thread_get_id(),
                          memory_order_relaxed);
#else
    (void) r;
#endif
}

void
sc_mutex_unlock(sc_mutex *mutex) {
#ifndef NDEBUG
    assert(sc_mutex_held(mutex));
    atomic_store_explicit(&mutex->locker, 0, memory_order_relaxed);
#endif
    int r = SDL_UnlockMutex(mutex->mutex);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not lock mutex: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

sc_thread_id
sc_thread_get_id(void) {
    return SDL_ThreadID();
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
    SDL_cond *sdl_cond = SDL_CreateCond();
    if (!sdl_cond) {
        LOG_OOM();
        return false;
    }

    cond->cond = sdl_cond;
    return true;
}

void
sc_cond_destroy(sc_cond *cond) {
    SDL_DestroyCond(cond->cond);
}

void
sc_cond_wait(sc_cond *cond, sc_mutex *mutex) {
    int r = SDL_CondWait(cond->cond, mutex->mutex);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not wait on condition: %s", SDL_GetError());
        abort();
    }

    atomic_store_explicit(&mutex->locker, sc_thread_get_id(),
                          memory_order_relaxed);
#else
    (void) r;
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
    int r = SDL_CondWaitTimeout(cond->cond, mutex->mutex, ms);
#ifndef NDEBUG
    if (r < 0) {
        LOGE("Could not wait on condition with timeout: %s", SDL_GetError());
        abort();
    }

    atomic_store_explicit(&mutex->locker, sc_thread_get_id(),
                          memory_order_relaxed);
#endif
    assert(r == 0 || r == SDL_MUTEX_TIMEDOUT);
    // The deadline is reached on timeout
    assert(r != SDL_MUTEX_TIMEDOUT || sc_tick_now() >= deadline);
    return r == 0;
}

void
sc_cond_signal(sc_cond *cond) {
    int r = SDL_CondSignal(cond->cond);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not signal a condition: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

void
sc_cond_broadcast(sc_cond *cond) {
    int r = SDL_CondBroadcast(cond->cond);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not broadcast a condition: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}
