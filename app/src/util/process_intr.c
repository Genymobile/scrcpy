#include "process_intr.h"

bool
sc_process_check_success_intr(struct sc_intr *intr, sc_pid pid,
                              const char *name) {
    if (!sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return false;
    }

    // Always pass close=false, interrupting would be racy otherwise
    bool ret = sc_process_check_success(pid, name, false);

    sc_intr_set_process(intr, SC_PROCESS_NONE);
    return ret;
}
