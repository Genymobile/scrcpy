#include "process.h"

#include <libgen.h>
#include "log.h"

enum process_result
process_execute(const char *const argv[], process_t *pid) {
    return process_execute_redirect(argv, pid, NULL, NULL, NULL);
}

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

ssize_t
read_pipe_all(pipe_t pipe, char *data, size_t len) {
    size_t copied = 0;
    while (len > 0) {
        ssize_t r = read_pipe(pipe, data, len);
        if (r <= 0) {
            return copied ? (ssize_t) copied : r;
        }
        len -= r;
        data += r;
        copied += r;
    }
    return copied;
}
