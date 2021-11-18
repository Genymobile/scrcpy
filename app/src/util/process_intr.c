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
