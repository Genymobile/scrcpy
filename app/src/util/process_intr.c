#include "process_intr.h"

bool
sc_process_check_success_intr(struct sc_intr *intr, sc_pid pid,
                              const char *name, bool close) {
    if (!sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return false;
    }

    // Always pass close=false, interrupting would be racy otherwise
    bool ret = sc_process_check_success(pid, name, false);

    sc_intr_set_process(intr, SC_PROCESS_NONE);

    if (close) {
        // Close separately
        sc_process_close(pid);
    }

    return ret;
}

ssize_t
sc_pipe_read_intr(struct sc_intr *intr, sc_pid pid, sc_pipe pipe, char *data,
                  size_t len) {
    if (!sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return false;
    }

    ssize_t ret = sc_pipe_read(pipe, data, len);

    sc_intr_set_process(intr, SC_PROCESS_NONE);
    return ret;
}

ssize_t
sc_pipe_read_all_intr(struct sc_intr *intr, sc_pid pid, sc_pipe pipe,
                      char *data, size_t len) {
    if (!sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return false;
    }

    ssize_t ret = sc_pipe_read_all(pipe, data, len);

    sc_intr_set_process(intr, SC_PROCESS_NONE);
    return ret;
}
