#ifndef SC_THREAD_H
#define SC_THREAD_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>

#include "tick.h"

/* Forward declarations */
typedef struct SDL_Thread SDL_Thread;
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_cond SDL_cond;

typedef int sc_thread_fn(void *);
typedef unsigned sc_thread_id;
typedef atomic_uint sc_atomic_thread_id;

typedef struct sc_thread {
    SDL_Thread *thread;
} sc_thread;

enum sc_thread_priority {
    SC_THREAD_PRIORITY_LOW,
    SC_THREAD_PRIORITY_NORMAL,
    SC_THREAD_PRIORITY_HIGH,
    SC_THREAD_PRIORITY_TIME_CRITICAL,
};

typedef struct sc_mutex {
    SDL_mutex *mutex;
#ifndef NDEBUG
    sc_atomic_thread_id locker;
#endif
} sc_mutex;

typedef struct sc_cond {
    SDL_cond *cond;
} sc_cond;

extern sc_thread_id SC_MAIN_THREAD_ID;

bool
sc_thread_create(sc_thread *thread, sc_thread_fn fn, const char *name,
                 void *userdata);

void
sc_thread_join(sc_thread *thread, int *status);

bool
sc_thread_set_priority(enum sc_thread_priority priority);

bool
sc_mutex_init(sc_mutex *mutex);

void
sc_mutex_destroy(sc_mutex *mutex);

void
sc_mutex_lock(sc_mutex *mutex);

void
sc_mutex_unlock(sc_mutex *mutex);

sc_thread_id
sc_thread_get_id(void);

#ifndef NDEBUG
bool
sc_mutex_held(struct sc_mutex *mutex);
# define sc_mutex_assert(mutex) assert(sc_mutex_held(mutex))
#else
# define sc_mutex_assert(mutex)
#endif

bool
sc_cond_init(sc_cond *cond);

void
sc_cond_destroy(sc_cond *cond);

void
sc_cond_wait(sc_cond *cond, sc_mutex *mutex);

// return true on signaled, false on timeout
bool
sc_cond_timedwait(sc_cond *cond, sc_mutex *mutex, sc_tick deadline);

void
sc_cond_signal(sc_cond *cond);

void
sc_cond_broadcast(sc_cond *cond);

#endif
