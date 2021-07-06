#include "fps_counter.h"

#include <assert.h>
#include <SDL2/SDL_timer.h>

#include "util/log.h"

#define FPS_COUNTER_INTERVAL_MS 1000

bool
fps_counter_init(struct fps_counter *counter) {
    bool ok = sc_mutex_init(&counter->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&counter->state_cond);
    if (!ok) {
        sc_mutex_destroy(&counter->mutex);
        return false;
    }

    counter->thread_started = false;
    atomic_init(&counter->started, 0);
    // no need to initialize the other fields, they are unused until started

    return true;
}

void
fps_counter_destroy(struct fps_counter *counter) {
    sc_cond_destroy(&counter->state_cond);
    sc_mutex_destroy(&counter->mutex);
}

static inline bool
is_started(struct fps_counter *counter) {
    return atomic_load_explicit(&counter->started, memory_order_acquire);
}

static inline void
set_started(struct fps_counter *counter, bool started) {
    atomic_store_explicit(&counter->started, started, memory_order_release);
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

    sc_mutex_lock(&counter->mutex);
    while (!counter->interrupted) {
        while (!counter->interrupted && !is_started(counter)) {
            sc_cond_wait(&counter->state_cond, &counter->mutex);
        }
        while (!counter->interrupted && is_started(counter)) {
            uint32_t now = SDL_GetTicks();
            check_interval_expired(counter, now);

            assert(counter->next_timestamp > now);
            uint32_t remaining = counter->next_timestamp - now;

            // ignore the reason (timeout or signaled), we just loop anyway
            sc_cond_timedwait(&counter->state_cond, &counter->mutex, remaining);
        }
    }
    sc_mutex_unlock(&counter->mutex);
    return 0;
}

bool
fps_counter_start(struct fps_counter *counter) {
    sc_mutex_lock(&counter->mutex);
    counter->next_timestamp = SDL_GetTicks() + FPS_COUNTER_INTERVAL_MS;
    counter->nr_rendered = 0;
    counter->nr_skipped = 0;
    sc_mutex_unlock(&counter->mutex);

    set_started(counter, true);
    sc_cond_signal(&counter->state_cond);

    // counter->thread_started and counter->thread are always accessed from the
    // same thread, no need to lock
    if (!counter->thread_started) {
        bool ok = sc_thread_create(&counter->thread, run_fps_counter,
                                   "fps counter", counter);
        if (!ok) {
            LOGE("Could not start FPS counter thread");
            return false;
        }

        counter->thread_started = true;
    }

    return true;
}

void
fps_counter_stop(struct fps_counter *counter) {
    set_started(counter, false);
    sc_cond_signal(&counter->state_cond);
}

bool
fps_counter_is_started(struct fps_counter *counter) {
    return is_started(counter);
}

void
fps_counter_interrupt(struct fps_counter *counter) {
    if (!counter->thread_started) {
        return;
    }

    sc_mutex_lock(&counter->mutex);
    counter->interrupted = true;
    sc_mutex_unlock(&counter->mutex);
    // wake up blocking wait
    sc_cond_signal(&counter->state_cond);
}

void
fps_counter_join(struct fps_counter *counter) {
    if (counter->thread_started) {
        // interrupted must be set by the thread calling join(), so no need to
        // lock for the assertion
        assert(counter->interrupted);

        sc_thread_join(&counter->thread, NULL);
    }
}

void
fps_counter_add_rendered_frame(struct fps_counter *counter) {
    if (!is_started(counter)) {
        return;
    }

    sc_mutex_lock(&counter->mutex);
    uint32_t now = SDL_GetTicks();
    check_interval_expired(counter, now);
    ++counter->nr_rendered;
    sc_mutex_unlock(&counter->mutex);
}

void
fps_counter_add_skipped_frame(struct fps_counter *counter) {
    if (!is_started(counter)) {
        return;
    }

    sc_mutex_lock(&counter->mutex);
    uint32_t now = SDL_GetTicks();
    check_interval_expired(counter, now);
    ++counter->nr_skipped;
    sc_mutex_unlock(&counter->mutex);
}
