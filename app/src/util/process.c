#include "process.h"

#include <assert.h>
#include <libgen.h>
#include "log.h"

enum sc_process_result
sc_process_execute(const char *const argv[], sc_pid *pid, unsigned flags) {
    return sc_process_execute_p(argv, pid, flags, NULL, NULL, NULL);
}

ssize_t
sc_pipe_read_all(sc_pipe pipe, char *data, size_t len) {
    size_t copied = 0;
    while (len > 0) {
        ssize_t r = sc_pipe_read(pipe, data, len);
        if (r <= 0) {
            return copied ? (ssize_t) copied : r;
        }
        len -= r;
        data += r;
        copied += r;
    }
    return copied;
}

static int
run_observer(void *data) {
    struct sc_process_observer *observer = data;
    sc_process_wait(observer->pid, false); // ignore exit code

    sc_mutex_lock(&observer->mutex);
    observer->terminated = true;
    sc_cond_signal(&observer->cond_terminated);
    sc_mutex_unlock(&observer->mutex);

    if (observer->listener) {
        observer->listener->on_terminated(observer->listener_userdata);
    }

    return 0;
}

bool
sc_process_observer_init(struct sc_process_observer *observer, sc_pid pid,
                         const struct sc_process_listener *listener,
                         void *listener_userdata) {
    // Either no listener, or on_terminated() is defined
    assert(!listener || listener->on_terminated);

    bool ok = sc_mutex_init(&observer->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&observer->cond_terminated);
    if (!ok) {
        sc_mutex_destroy(&observer->mutex);
        return false;
    }

    observer->pid = pid;
    observer->listener = listener;
    observer->listener_userdata = listener_userdata;
    observer->terminated = false;

    ok = sc_thread_create(&observer->thread, run_observer, "scrcpy-proc",
                          observer);
    if (!ok) {
        sc_cond_destroy(&observer->cond_terminated);
        sc_mutex_destroy(&observer->mutex);
        return false;
    }

    return true;
}

bool
sc_process_observer_timedwait(struct sc_process_observer *observer,
                              sc_tick deadline) {
    sc_mutex_lock(&observer->mutex);
    bool timed_out = false;
    while (!observer->terminated && !timed_out) {
        timed_out = !sc_cond_timedwait(&observer->cond_terminated,
                                       &observer->mutex, deadline);
    }
    bool terminated = observer->terminated;
    sc_mutex_unlock(&observer->mutex);

    return terminated;
}

void
sc_process_observer_join(struct sc_process_observer *observer) {
    sc_thread_join(&observer->thread, NULL);
}

void
sc_process_observer_destroy(struct sc_process_observer *observer) {
    sc_cond_destroy(&observer->cond_terminated);
    sc_mutex_destroy(&observer->mutex);
}
