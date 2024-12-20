#include "intr.h"

#include <assert.h>

#include "util/log.h"

bool
sc_intr_init(struct sc_intr *intr) {
    bool ok = sc_mutex_init(&intr->mutex);
    if (!ok) {
        LOG_OOM();
        return false;
    }

    intr->socket = SC_SOCKET_NONE;
    intr->process = SC_PROCESS_NONE;

    atomic_store_explicit(&intr->interrupted, false, memory_order_relaxed);

    return true;
}

bool
sc_intr_set_socket(struct sc_intr *intr, sc_socket socket) {
    assert(intr->process == SC_PROCESS_NONE);

    sc_mutex_lock(&intr->mutex);
    bool interrupted =
        atomic_load_explicit(&intr->interrupted, memory_order_relaxed);
    if (!interrupted) {
        intr->socket = socket;
    }
    sc_mutex_unlock(&intr->mutex);

    return !interrupted;
}

bool
sc_intr_set_process(struct sc_intr *intr, sc_pid pid) {
    assert(intr->socket == SC_SOCKET_NONE);

    sc_mutex_lock(&intr->mutex);
    bool interrupted =
        atomic_load_explicit(&intr->interrupted, memory_order_relaxed);
    if (!interrupted) {
        intr->process = pid;
    }
    sc_mutex_unlock(&intr->mutex);

    return !interrupted;
}

void
sc_intr_interrupt(struct sc_intr *intr) {
    sc_mutex_lock(&intr->mutex);

    atomic_store_explicit(&intr->interrupted, true, memory_order_relaxed);

    // No more than one component to interrupt
    assert(intr->socket == SC_SOCKET_NONE ||
           intr->process == SC_PROCESS_NONE);

    if (intr->socket != SC_SOCKET_NONE) {
        LOGD("Interrupting socket");
        net_interrupt(intr->socket);
        intr->socket = SC_SOCKET_NONE;
    }
    if (intr->process != SC_PROCESS_NONE) {
        LOGD("Interrupting process");
        sc_process_terminate(intr->process);
        intr->process = SC_PROCESS_NONE;
    }

    sc_mutex_unlock(&intr->mutex);
}

void
sc_intr_destroy(struct sc_intr *intr) {
    assert(intr->socket == SC_SOCKET_NONE);
    assert(intr->process == SC_PROCESS_NONE);

    sc_mutex_destroy(&intr->mutex);
}
