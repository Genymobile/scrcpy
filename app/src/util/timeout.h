#ifndef SC_TIMEOUT_H
#define SC_TIMEOUT_H

#include "common.h"

#include <stdbool.h>

#include "util/thread.h"
#include "util/tick.h"

struct sc_timeout {
    sc_thread thread;
    sc_tick deadline;

    sc_mutex mutex;
    sc_cond cond;
    bool stopped;

    const struct sc_timeout_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_timeout_callbacks {
    void (*on_timeout)(struct sc_timeout *timeout, void *userdata);
};

bool
sc_timeout_init(struct sc_timeout *timeout);

void
sc_timeout_destroy(struct sc_timeout *timeout);

bool
sc_timeout_start(struct sc_timeout *timeout, sc_tick deadline,
                 const struct sc_timeout_callbacks *cbs, void *cbs_userdata);

void
sc_timeout_stop(struct sc_timeout *timeout);

void
sc_timeout_join(struct sc_timeout *timeout);

#endif
