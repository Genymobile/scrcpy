#include <stdlib.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_mutex.h>

void mutex_lock(SDL_mutex *mutex) {
    if (SDL_LockMutex(mutex)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Could not lock mutex");
        abort();
    }
}

void mutex_unlock(SDL_mutex *mutex) {
    if (SDL_UnlockMutex(mutex)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Could not unlock mutex");
        abort();
    }
}

void cond_wait(SDL_cond *cond, SDL_mutex *mutex) {
    if (SDL_CondWait(cond, mutex)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Could not wait on condition");
        abort();
    }
}

void cond_signal(SDL_cond *cond) {
    if (SDL_CondSignal(cond)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Could not signal a condition");
        abort();
    }
}

