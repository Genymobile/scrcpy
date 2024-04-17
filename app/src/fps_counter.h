#ifndef SC_FPSCOUNTER_H
#define SC_FPSCOUNTER_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "util/thread.h"

struct sc_fps_counter {
    sc_thread thread;
    sc_mutex mutex;
    sc_cond state_cond;

    bool thread_started;

    // atomic so that we can check without locking the mutex
    // if the FPS counter is disabled, we don't want to lock unnecessarily
    atomic_bool started;

    // the following fields are protected by the mutex
    bool interrupted;
    unsigned nr_rendered;
    unsigned nr_skipped;
    sc_tick next_timestamp;
};

bool
sc_fps_counter_init(struct sc_fps_counter *counter);

void
sc_fps_counter_destroy(struct sc_fps_counter *counter);

bool
sc_fps_counter_start(struct sc_fps_counter *counter);

void
sc_fps_counter_stop(struct sc_fps_counter *counter);

bool
sc_fps_counter_is_started(struct sc_fps_counter *counter);

// request to stop the thread (on quit)
// must be called before sc_fps_counter_join()
void
sc_fps_counter_interrupt(struct sc_fps_counter *counter);

void
sc_fps_counter_join(struct sc_fps_counter *counter);

void
sc_fps_counter_add_rendered_frame(struct sc_fps_counter *counter);

void
sc_fps_counter_add_skipped_frame(struct sc_fps_counter *counter);

#endif
