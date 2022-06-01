#ifndef SC_RECEIVER_H
#define SC_RECEIVER_H

#include "common.h"

#include <stdbool.h>

#include "util/acksync.h"
#include "util/net.h"
#include "util/thread.h"

// receive events from the device
// managed by the controller
struct receiver {
    sc_socket control_socket;
    sc_thread thread;
    sc_mutex mutex;

    struct sc_acksync *acksync;
};

bool
receiver_init(struct receiver *receiver, sc_socket control_socket,
              struct sc_acksync *acksync);

void
receiver_destroy(struct receiver *receiver);

bool
receiver_start(struct receiver *receiver);

// no receiver_stop(), it will automatically stop on control_socket shutdown

void
receiver_join(struct receiver *receiver);

#endif
