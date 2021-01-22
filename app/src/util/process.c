#include "process.h"

#include "log.h"

bool
process_check_success(process_t proc, const char *name, bool close) {
    if (proc == PROCESS_NONE) {
        LOGE("Could not execute \"%s\"", name);
        return false;
    }
    exit_code_t exit_code = process_wait(proc, close);
    if (exit_code) {
        if (exit_code != NO_EXIT_CODE) {
            LOGE("\"%s\" returned with value %" PRIexitcode, name, exit_code);
        } else {
            LOGE("\"%s\" exited unexpectedly", name);
        }
        return false;
    }
    return true;
}
