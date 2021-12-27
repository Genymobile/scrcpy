#ifndef SC_ACK_SYNC_H
#define SC_ACK_SYNC_H

#include "common.h"

#include "thread.h"

#define SC_SEQUENCE_INVALID 0

/**
 * Helper to wait for acknowledgments
 *
 * In practice, it is used to wait for device clipboard acknowledgement from the
 * server before injecting Ctrl+v via AOA HID, in order to avoid pasting the
 * content of the old device clipboard (if Ctrl+v was injected before the
 * clipboard content was actually set).
 */
struct sc_acksync {
    sc_mutex mutex;
    sc_cond cond;

    bool stopped;

    // Last acked value, initially SC_SEQUENCE_INVALID
    uint64_t ack;
};

enum sc_acksync_wait_result {
    // Acknowledgment received
    SC_ACKSYNC_WAIT_OK,

    // Timeout expired
    SC_ACKSYNC_WAIT_TIMEOUT,

    // Interrupted from another thread by sc_acksync_interrupt()
    SC_ACKSYNC_WAIT_INTR,
};

bool
sc_acksync_init(struct sc_acksync *as);

void
sc_acksync_destroy(struct sc_acksync *as);

/**
 * Acknowledge `sequence`
 *
 * The `sequence` must be greater than (or equal to) any previous acknowledged
 * sequence.
 */
void
sc_acksync_ack(struct sc_acksync *as, uint64_t sequence);

/**
 * Wait for acknowledgment of sequence `ack` (or higher)
 */
enum sc_acksync_wait_result
sc_acksync_wait(struct sc_acksync *as, uint64_t ack, sc_tick deadline);

/**
 * Interrupt any `sc_acksync_wait()`
 */
void
sc_acksync_interrupt(struct sc_acksync *as);

#endif
