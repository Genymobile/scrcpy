#ifndef RECEIVER_H
#define RECEIVER_H

#include "common.h"

#include <stdbool.h>

#include "util/net.h"
#include "util/thread.h"

// receive events from the device
// managed by the controller
struct receiver {
    socket_t control_socket;
    sc_thread thread;
    sc_mutex mutex;
};

bool
receiver_init(struct receiver *receiver, socket_t control_socket);

void
receiver_destroy(struct receiver *receiver);

bool
receiver_start(struct receiver *receiver);

// no receiver_stop(), it will automatically stop on control_socket shutdown

void
receiver_join(struct receiver *receiver);

#endif
