#ifndef LOCKUTIL_H
#define LOCKUTIL_H

static inline void lock_mutex(SDL_mutex *mutex) {
    if (SDL_LockMutex(mutex)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not lock mutex");
        exit(1);
    }
}

static inline void unlock_mutex(SDL_mutex *mutex) {
    if (SDL_UnlockMutex(mutex)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not unlock mutex");
        exit(1);
    }
}

#endif
