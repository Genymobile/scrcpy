#include "process.h"

#include <libgen.h>
#include "log.h"

enum sc_process_result
sc_process_execute(const char *const argv[], sc_pid *pid) {
    return sc_process_execute_p(argv, pid, NULL, NULL, NULL);
}

bool
sc_process_check_success(sc_pid pid, const char *name, bool close) {
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"%s\"", name);
        return false;
    }
    sc_exit_code exit_code = sc_process_wait(pid, close);
    if (exit_code) {
        if (exit_code != SC_EXIT_CODE_NONE) {
            LOGE("\"%s\" returned with value %" SC_PRIexitcode, name,
                 exit_code);
        } else {
            LOGE("\"%s\" exited unexpectedly", name);
        }
        return false;
    }
    return true;
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
