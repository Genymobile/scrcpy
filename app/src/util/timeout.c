#include "timeout.h"

#include <assert.h>
#include <stddef.h>

#include "util/log.h"

bool
sc_timeout_init(struct sc_timeout *timeout) {
    bool ok = sc_mutex_init(&timeout->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&timeout->cond);
    if (!ok) {
        return false;
    }

    timeout->stopped = false;

    return true;
}

static int
run_timeout(void *data) {
    struct sc_timeout *timeout = data;
    sc_tick deadline = timeout->deadline;

    sc_mutex_lock(&timeout->mutex);
    bool timed_out = false;
    while (!timeout->stopped && !timed_out) {
        timed_out = !sc_cond_timedwait(&timeout->cond, &timeout->mutex,
                    deadline);
    }
    sc_mutex_unlock(&timeout->mutex);

    timeout->cbs->on_timeout(timeout, timeout->cbs_userdata);

    return 0;
}

bool
sc_timeout_start(struct sc_timeout *timeout, sc_tick deadline,
                 const struct sc_timeout_callbacks *cbs, void *cbs_userdata) {
    bool ok = sc_thread_create(&timeout->thread, run_timeout, "scrcpy-timeout",
                               timeout);
    if (!ok) {
        LOGE("Timeout: could not start thread");
        return false;
    }

    timeout->deadline = deadline;

    assert(cbs && cbs->on_timeout);
    timeout->cbs = cbs;
    timeout->cbs_userdata = cbs_userdata;

    return true;
}

void
sc_timeout_stop(struct sc_timeout *timeout) {
    sc_mutex_lock(&timeout->mutex);
    timeout->stopped = true;
    sc_cond_signal(&timeout->cond);
    sc_mutex_unlock(&timeout->mutex);
}

void
sc_timeout_join(struct sc_timeout *timeout) {
    sc_thread_join(&timeout->thread, NULL);
}

void
sc_timeout_destroy(struct sc_timeout *timeout) {
    sc_mutex_destroy(&timeout->mutex);
    sc_cond_destroy(&timeout->cond);
}
