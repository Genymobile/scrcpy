#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "common.h"

#include <stdbool.h>

#include "control_msg.h"
#include "receiver.h"
#include "util/acksync.h"
#include "util/cbuf.h"
#include "util/net.h"
#include "util/thread.h"

struct control_msg_queue CBUF(struct control_msg, 64);

struct controller {
    sc_socket control_socket;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond msg_cond;
    bool stopped;
    struct control_msg_queue queue;
    struct receiver receiver;
};

bool
controller_init(struct controller *controller, sc_socket control_socket,
                struct sc_acksync *acksync);

void
controller_destroy(struct controller *controller);

bool
controller_start(struct controller *controller);

void
controller_stop(struct controller *controller);

void
controller_join(struct controller *controller);

bool
controller_push_msg(struct controller *controller,
                    const struct control_msg *msg);

#endif
