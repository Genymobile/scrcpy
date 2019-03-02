#ifndef LOCKUTIL_H
#define LOCKUTIL_H

// forward declarations
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_cond SDL_cond;

void
mutex_lock(SDL_mutex *mutex);

void
mutex_unlock(SDL_mutex *mutex);

void
cond_wait(SDL_cond *cond, SDL_mutex *mutex);

void
cond_signal(SDL_cond *cond);

#endif
