#ifndef SC_CONTROLLER_H
#define SC_CONTROLLER_H

#include "common.h"

#include <stdbool.h>

#include "control_msg.h"
#include "receiver.h"
#include "util/acksync.h"
#include "util/cbuf.h"
#include "util/net.h"
#include "util/thread.h"

struct sc_control_msg_queue CBUF(struct sc_control_msg, 64);

struct sc_controller {
    sc_socket control_socket;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond msg_cond;
    bool stopped;
    struct sc_control_msg_queue queue;
    struct receiver receiver;
};

bool
sc_controller_init(struct sc_controller *controller, sc_socket control_socket,
                   struct sc_acksync *acksync);

void
sc_controller_destroy(struct sc_controller *controller);

bool
sc_controller_start(struct sc_controller *controller);

void
sc_controller_stop(struct sc_controller *controller);

void
sc_controller_join(struct sc_controller *controller);

bool
sc_controller_push_msg(struct sc_controller *controller,
                       const struct sc_control_msg *msg);

#endif
