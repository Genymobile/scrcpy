#ifndef LOCK_H
#define LOCK_H

#include <stdint.h>
#include <SDL2/SDL_mutex.h>

#include "config.h"
#include "log.h"

static inline void
mutex_lock(SDL_mutex *mutex) {
    int r = SDL_LockMutex(mutex);
#ifndef NDEBUG
    if (r) {
        LOGC("Could not lock mutex: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

static inline void
mutex_unlock(SDL_mutex *mutex) {
    int r = SDL_UnlockMutex(mutex);
#ifndef NDEBUG
    if (r) {
        LOGC("Could not unlock mutex: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

static inline void
cond_wait(SDL_cond *cond, SDL_mutex *mutex) {
    int r = SDL_CondWait(cond, mutex);
#ifndef NDEBUG
    if (r) {
        LOGC("Could not wait on condition: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

static inline int
cond_wait_timeout(SDL_cond *cond, SDL_mutex *mutex, uint32_t ms) {
    int r = SDL_CondWaitTimeout(cond, mutex, ms);
#ifndef NDEBUG
    if (r < 0) {
        LOGC("Could not wait on condition with timeout: %s", SDL_GetError());
        abort();
    }
#endif
    return r;
}

static inline void
cond_signal(SDL_cond *cond) {
    int r = SDL_CondSignal(cond);
#ifndef NDEBUG
    if (r) {
        LOGC("Could not signal a condition: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

#endif
