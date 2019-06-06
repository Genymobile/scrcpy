#include "fps_counter.h"

#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_assert.h>

#include "lock_util.h"
#include "log.h"

#define FPS_COUNTER_INTERVAL 1000

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
    counter->started = false;
    counter->interrupted = false;
    counter->nr_rendered = 0;
    counter->nr_skipped = 0;
    // no need to initialize slice_start, it is meaningful only when started

    return true;
}

void
fps_counter_destroy(struct fps_counter *counter) {
    SDL_DestroyCond(counter->state_cond);
    SDL_DestroyMutex(counter->mutex);
}

static void
display_fps(struct fps_counter *counter) {
    unsigned rendered_per_second =
        counter->nr_rendered * 1000 / FPS_COUNTER_INTERVAL;
    if (counter->nr_skipped) {
        unsigned skipped_per_second =
            counter->nr_skipped * 1000 / FPS_COUNTER_INTERVAL;
        LOGI("%u fps (+%u frames skipped)", rendered_per_second,
                                            skipped_per_second);
    } else {
        LOGI("%u fps", rendered_per_second);
    }
}

static int
run_fps_counter(void *data) {
    struct fps_counter *counter = data;

    mutex_lock(counter->mutex);
    while (!counter->interrupted) {
        while (!counter->interrupted && !counter->started) {
            cond_wait(counter->state_cond, counter->mutex);
        }
        while (!counter->interrupted && counter->started) {
            uint32_t now = SDL_GetTicks();
            if (now >= counter->next_timestamp) {
                display_fps(counter);
                counter->nr_rendered = 0;
                counter->nr_skipped = 0;
                // add a multiple of the interval
                uint32_t elapsed_slices =
                    (now - counter->next_timestamp) / FPS_COUNTER_INTERVAL + 1;
                counter->next_timestamp += FPS_COUNTER_INTERVAL * elapsed_slices;
            }
            SDL_assert(counter->next_timestamp > now);
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
    if (!counter->thread) {
        counter->thread =
            SDL_CreateThread(run_fps_counter, "fps counter", counter);
        if (!counter->thread) {
            LOGE("Could not start FPS counter thread");
            return false;
        }
    }

    mutex_lock(counter->mutex);
    counter->started = true;
    counter->next_timestamp = SDL_GetTicks() + FPS_COUNTER_INTERVAL;
    counter->nr_rendered = 0;
    counter->nr_skipped = 0;
    mutex_unlock(counter->mutex);

    cond_signal(counter->state_cond);
    return true;
}

void
fps_counter_stop(struct fps_counter *counter) {
    mutex_lock(counter->mutex);
    counter->started = false;
    mutex_unlock(counter->mutex);
    cond_signal(counter->state_cond);
}

void
fps_counter_interrupt(struct fps_counter *counter) {
    if (counter->thread) {
        mutex_lock(counter->mutex);
        counter->interrupted = true;
        mutex_unlock(counter->mutex);
        // wake up blocking wait
        cond_signal(counter->state_cond);
    }
}

void
fps_counter_join(struct fps_counter *counter) {
    if (counter->thread) {
        SDL_WaitThread(counter->thread, NULL);
    }
}

void
fps_counter_add_rendered_frame(struct fps_counter *counter) {
    ++counter->nr_rendered;
}

void
fps_counter_add_skipped_frame(struct fps_counter *counter) {
    ++counter->nr_skipped;
}
