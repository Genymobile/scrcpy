#include "process_intr.h"

ssize_t
sc_pipe_read_intr(struct sc_intr *intr, sc_pid pid, sc_pipe pipe, char *data,
                  size_t len) {
    if (intr && !sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return -1;
    }

    ssize_t ret = sc_pipe_read(pipe, data, len);

    if (intr) {
        sc_intr_set_process(intr, SC_PROCESS_NONE);
    }

    return ret;
}

ssize_t
sc_pipe_read_all_intr(struct sc_intr *intr, sc_pid pid, sc_pipe pipe,
                      char *data, size_t len) {
    if (intr && !sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return -1;
    }

    ssize_t ret = sc_pipe_read_all(pipe, data, len);

    if (intr) {
        sc_intr_set_process(intr, SC_PROCESS_NONE);
    }

    return ret;
}
