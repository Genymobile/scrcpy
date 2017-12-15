#ifndef LOCKUTIL_H
#define LOCKUTIL_H

#include <stdlib.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_mutex.h>

static inline void mutex_lock(SDL_mutex *mutex) {
    if (SDL_LockMutex(mutex)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not lock mutex");
        exit(1);
    }
}

static inline void mutex_unlock(SDL_mutex *mutex) {
    if (SDL_UnlockMutex(mutex)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not unlock mutex");
        exit(1);
    }
}

#endif
