#ifndef FPSCOUNTER_H
#define FPSCOUNTER_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "util/thread.h"

struct fps_counter {
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
fps_counter_init(struct fps_counter *counter);

void
fps_counter_destroy(struct fps_counter *counter);

bool
fps_counter_start(struct fps_counter *counter);

void
fps_counter_stop(struct fps_counter *counter);

bool
fps_counter_is_started(struct fps_counter *counter);

// request to stop the thread (on quit)
// must be called before fps_counter_join()
void
fps_counter_interrupt(struct fps_counter *counter);

void
fps_counter_join(struct fps_counter *counter);

void
fps_counter_add_rendered_frame(struct fps_counter *counter);

void
fps_counter_add_skipped_frame(struct fps_counter *counter);

#endif
