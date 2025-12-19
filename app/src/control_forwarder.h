#ifndef SC_CONTROL_FORWARDER_H
#define SC_CONTROL_FORWARDER_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "controller.h"
#include "util/net.h"
#include "util/thread.h"

struct sc_control_forwarder {
    uint16_t port;
    
    sc_socket server_socket;
    sc_socket client_socket;
    
    sc_thread thread;
    sc_mutex mutex;
    
    bool stopped;
    
    struct sc_controller *controller;
};

bool
sc_control_forwarder_init(struct sc_control_forwarder *forwarder, uint16_t port);

bool
sc_control_forwarder_start(struct sc_control_forwarder *forwarder,
                           struct sc_controller *controller);

void
sc_control_forwarder_stop(struct sc_control_forwarder *forwarder);

void
sc_control_forwarder_join(struct sc_control_forwarder *forwarder);

void
sc_control_forwarder_destroy(struct sc_control_forwarder *forwarder);

#endif
