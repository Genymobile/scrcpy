#include "acksync.h"

#include <assert.h>
#include "util/log.h"

bool
sc_acksync_init(struct sc_acksync *as) {
    bool ok = sc_mutex_init(&as->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&as->cond);
    if (!ok) {
        sc_mutex_destroy(&as->mutex);
        return false;
    }

    as->stopped = false;
    as->ack = SC_SEQUENCE_INVALID;

    return true;
}

void
sc_acksync_destroy(struct sc_acksync *as) {
    sc_cond_destroy(&as->cond);
    sc_mutex_destroy(&as->mutex);
}

void
sc_acksync_ack(struct sc_acksync *as, uint64_t sequence) {
    sc_mutex_lock(&as->mutex);

    // Acknowledgements must be monotonic
    assert(sequence >= as->ack);

    as->ack = sequence;
    sc_cond_signal(&as->cond);

    sc_mutex_unlock(&as->mutex);
}

enum sc_acksync_wait_result
sc_acksync_wait(struct sc_acksync *as, uint64_t ack, sc_tick deadline) {
    sc_mutex_lock(&as->mutex);

    bool timed_out = false;
    while (!as->stopped && as->ack < ack && !timed_out) {
        timed_out = !sc_cond_timedwait(&as->cond, &as->mutex, deadline);
    }

    enum sc_acksync_wait_result ret;
    if (as->stopped) {
        ret = SC_ACKSYNC_WAIT_INTR;
    } else if (as->ack >= ack) {
        ret = SC_ACKSYNC_WAIT_OK;
    } else {
        assert(timed_out);
        ret = SC_ACKSYNC_WAIT_TIMEOUT;
    }
    sc_mutex_unlock(&as->mutex);

    return ret;
}

/**
 * Interrupt any `sc_acksync_wait()`
 */
void
sc_acksync_interrupt(struct sc_acksync *as) {
    sc_mutex_lock(&as->mutex);
    as->stopped = true;
    sc_cond_signal(&as->cond);
    sc_mutex_unlock(&as->mutex);
}
