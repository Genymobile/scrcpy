#ifndef SC_CONTROLLER_H
#define SC_CONTROLLER_H

#include "common.h"

#include <stdbool.h>

#include "control_msg.h"
#include "receiver.h"
#include "util/acksync.h"
#include "util/net.h"
#include "util/thread.h"
#include "util/vecdeque.h"

struct sc_control_msg_queue SC_VECDEQUE(struct sc_control_msg);

struct sc_controller {
    sc_socket control_socket;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond msg_cond;
    bool stopped;
    struct sc_control_msg_queue queue;
    struct sc_receiver receiver;
};

bool
sc_controller_init(struct sc_controller *controller, sc_socket control_socket);

void
sc_controller_configure(struct sc_controller *controller,
                        struct sc_acksync *acksync,
                        struct sc_uhid_devices *uhid_devices);

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
