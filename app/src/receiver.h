#ifndef SC_RECEIVER_H
#define SC_RECEIVER_H

#include "common.h"

#include <stdbool.h>

#include "util/acksync.h"
#include "util/net.h"
#include "util/thread.h"

// receive events from the device
// managed by the controller
struct sc_receiver {
    sc_socket control_socket;
    sc_thread thread;
    sc_mutex mutex;

    struct sc_acksync *acksync;
};

bool
sc_receiver_init(struct sc_receiver *receiver, sc_socket control_socket,
                 struct sc_acksync *acksync);

void
sc_receiver_destroy(struct sc_receiver *receiver);

bool
sc_receiver_start(struct sc_receiver *receiver);

// no sc_receiver_stop(), it will automatically stop on control_socket shutdown

void
sc_receiver_join(struct sc_receiver *receiver);

#endif
