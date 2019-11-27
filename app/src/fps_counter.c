#include "fps_counter.h"

#include <assert.h>
#include <SDL2/SDL_timer.h>

#include "config.h"
#include "util/lock.h"
#include "util/log.h"

#define FPS_COUNTER_INTERVAL_MS 1000

bool
fps_counter_init(struct fps_counter *counter) {
    counter->mutex = SDL_CreateMutex();
    if (!counter->mutex) {
        return false;
    }

    counter->state_cond = SDL_CreateCond();
    if (!counter->state_cond) {
        SDL_DestroyMutex(counter->mutex);
        return false;
    }

    counter->thread = NULL;
    SDL_AtomicSet(&counter->started, 0);
    // no need to initialize the other fields, they are unused until started

    return true;
}

void
fps_counter_destroy(struct fps_counter *counter) {
    SDL_DestroyCond(counter->state_cond);
    SDL_DestroyMutex(counter->mutex);
}

// must be called with mutex locked
static void
display_fps(struct fps_counter *counter) {
    unsigned rendered_per_second =
        counter->nr_rendered * 1000 / FPS_COUNTER_INTERVAL_MS;
    if (counter->nr_skipped) {
        LOGI("%u fps (+%u frames skipped)", rendered_per_second,
                                            counter->nr_skipped);
    } else {
        LOGI("%u fps", rendered_per_second);
    }
}

// must be called with mutex locked
static void
check_interval_expired(struct fps_counter *counter, uint32_t now) {
    if (now < counter->next_timestamp) {
        return;
    }

    display_fps(counter);
    counter->nr_rendered = 0;
    counter->nr_skipped = 0;
    // add a multiple of the interval
    uint32_t elapsed_slices =
        (now - counter->next_timestamp) / FPS_COUNTER_INTERVAL_MS + 1;
    counter->next_timestamp += FPS_COUNTER_INTERVAL_MS * elapsed_slices;
}

static int
run_fps_counter(void *data) {
    struct fps_counter *counter = data;

    mutex_lock(counter->mutex);
    while (!counter->interrupted) {
        while (!counter->interrupted && !SDL_AtomicGet(&counter->started)) {
            cond_wait(counter->state_cond, counter->mutex);
        }
        while (!counter->interrupted && SDL_AtomicGet(&counter->started)) {
            uint32_t now = SDL_GetTicks();
            check_interval_expired(counter, now);

            assert(counter->next_timestamp > now);
            uint32_t remaining = counter->next_timestamp - now;

            // ignore the reason (timeout or signaled), we just loop anyway
            cond_wait_timeout(counter->state_cond, counter->mutex, remaining);
        }
    }
    mutex_unlock(counter->mutex);
    return 0;
}

bool
fps_counter_start(struct fps_counter *counter) {
    mutex_lock(counter->mutex);
    counter->next_timestamp = SDL_GetTicks() + FPS_COUNTER_INTERVAL_MS;
    counter->nr_rendered = 0;
    counter->nr_skipped = 0;
    mutex_unlock(counter->mutex);

    SDL_AtomicSet(&counter->started, 1);
    cond_signal(counter->state_cond);

    // counter->thread is always accessed from the same thread, no need to lock
    if (!counter->thread) {
        counter->thread =
            SDL_CreateThread(run_fps_counter, "fps counter", counter);
        if (!counter->thread) {
            LOGE("Could not start FPS counter thread");
            return false;
        }
    }

    return true;
}

void
fps_counter_stop(struct fps_counter *counter) {
    SDL_AtomicSet(&counter->started, 0);
    cond_signal(counter->state_cond);
}

bool
fps_counter_is_started(struct fps_counter *counter) {
    return SDL_AtomicGet(&counter->started);
}

void
fps_counter_interrupt(struct fps_counter *counter) {
    if (!counter->thread) {
        return;
    }

    mutex_lock(counter->mutex);
    counter->interrupted = true;
    mutex_unlock(counter->mutex);
    // wake up blocking wait
    cond_signal(counter->state_cond);
}

void
fps_counter_join(struct fps_counter *counter) {
    if (counter->thread) {
        SDL_WaitThread(counter->thread, NULL);
    }
}

void
fps_counter_add_rendered_frame(struct fps_counter *counter) {
    if (!SDL_AtomicGet(&counter->started)) {
        return;
    }

    mutex_lock(counter->mutex);
    uint32_t now = SDL_GetTicks();
    check_interval_expired(counter, now);
    ++counter->nr_rendered;
    mutex_unlock(counter->mutex);
}

void
fps_counter_add_skipped_frame(struct fps_counter *counter) {
    if (!SDL_AtomicGet(&counter->started)) {
        return;
    }

    mutex_lock(counter->mutex);
    uint32_t now = SDL_GetTicks();
    check_interval_expired(counter, now);
    ++counter->nr_skipped;
    mutex_unlock(counter->mutex);
}
