#ifndef SC_INTR_H
#define SC_INTR_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>

#include "util/net.h"
#include "util/process.h"
#include "util/thread.h"

/**
 * Interruptor to wake up a blocking call from another thread
 *
 * It allows to register a socket or a process before a blocking call, and
 * interrupt/close from another thread to wake up the blocking call.
 */
struct sc_intr {
    sc_mutex mutex;

    sc_socket socket;
    sc_pid process;

    // Written protected by the mutex to avoid race conditions against
    // sc_intr_set_socket() and sc_intr_set_process(), but can be read
    // (atomically) without mutex
    atomic_bool interrupted;
};

/**
 * Initialize an interruptor
 */
bool
sc_intr_init(struct sc_intr *intr);

/**
 * Set a socket as the interruptible component
 *
 * Call with SC_SOCKET_NONE to unset.
 */
bool
sc_intr_set_socket(struct sc_intr *intr, sc_socket socket);

/**
 * Set a process as the interruptible component
 *
 * Call with SC_PROCESS_NONE to unset.
 */
bool
sc_intr_set_process(struct sc_intr *intr, sc_pid socket);

/**
 * Interrupt the current interruptible component
 *
 * Must be called from a different thread.
 */
void
sc_intr_interrupt(struct sc_intr *intr);

/**
 * Read the interrupted state
 *
 * It is exposed as a static inline function because it just loads from an
 * atomic.
 */
static inline bool
sc_intr_is_interrupted(struct sc_intr *intr) {
    return atomic_load_explicit(&intr->interrupted, memory_order_relaxed);
}

/**
 * Destroy the interruptor
 */
void
sc_intr_destroy(struct sc_intr *intr);

#endif
