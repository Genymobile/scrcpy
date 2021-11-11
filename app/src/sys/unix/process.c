#include "util/process.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util/log.h"

enum process_result
process_execute_redirect(const char *const argv[], pid_t *pid, int *pipe_stdin,
                     int *pipe_stdout, int *pipe_stderr) {
    int in[2];
    int out[2];
    int err[2];
    int internal[2]; // communication between parent and children

    if (pipe(internal) == -1) {
        perror("pipe");
        return PROCESS_ERROR_GENERIC;
    }
    if (pipe_stdin) {
        if (pipe(in) == -1) {
            perror("pipe");
            close(internal[0]);
            close(internal[1]);
            return PROCESS_ERROR_GENERIC;
        }
    }
    if (pipe_stdout) {
        if (pipe(out) == -1) {
            perror("pipe");
            // clean up
            if (pipe_stdin) {
                close(in[0]);
                close(in[1]);
            }
            close(internal[0]);
            close(internal[1]);
            return PROCESS_ERROR_GENERIC;
        }
    }
    if (pipe_stderr) {
        if (pipe(err) == -1) {
            perror("pipe");
            // clean up
            if (pipe_stdout) {
                close(out[0]);
                close(out[1]);
            }
            if (pipe_stdin) {
                close(in[0]);
                close(in[1]);
            }
            close(internal[0]);
            close(internal[1]);
            return PROCESS_ERROR_GENERIC;
        }
    }

    *pid = fork();
    if (*pid == -1) {
        perror("fork");
        // clean up
        if (pipe_stderr) {
            close(err[0]);
            close(err[1]);
        }
        if (pipe_stdout) {
            close(out[0]);
            close(out[1]);
        }
        if (pipe_stdin) {
            close(in[0]);
            close(in[1]);
        }
        close(internal[0]);
        close(internal[1]);
        return PROCESS_ERROR_GENERIC;
    }

    if (*pid == 0) {
        if (pipe_stdin) {
            if (in[0] != STDIN_FILENO) {
                dup2(in[0], STDIN_FILENO);
                close(in[0]);
            }
            close(in[1]);
        }
        if (pipe_stdout) {
            if (out[1] != STDOUT_FILENO) {
                dup2(out[1], STDOUT_FILENO);
                close(out[1]);
            }
            close(out[0]);
        }
        if (pipe_stderr) {
            if (err[1] != STDERR_FILENO) {
                dup2(err[1], STDERR_FILENO);
                close(err[1]);
            }
            close(err[0]);
        }
        close(internal[0]);
        enum process_result err;
        if (fcntl(internal[1], F_SETFD, FD_CLOEXEC) == 0) {
            execvp(argv[0], (char *const *) argv);
            perror("exec");
            err = errno == ENOENT ? PROCESS_ERROR_MISSING_BINARY
                                  : PROCESS_ERROR_GENERIC;
        } else {
            perror("fcntl");
            err = PROCESS_ERROR_GENERIC;
        }
        // send err to the parent
        if (write(internal[1], &err, sizeof(err)) == -1) {
            perror("write");
        }
        close(internal[1]);
        _exit(1);
    }

    // parent
    assert(*pid > 0);

    close(internal[1]);

    enum process_result res = PROCESS_SUCCESS;
    // wait for EOF or receive err from child
    if (read(internal[0], &res, sizeof(res)) == -1) {
        perror("read");
        res = PROCESS_ERROR_GENERIC;
    }

    close(internal[0]);

    if (pipe_stdin) {
        close(in[0]);
        *pipe_stdin = in[1];
    }
    if (pipe_stdout) {
        *pipe_stdout = out[0];
        close(out[1]);
    }
    if (pipe_stderr) {
        *pipe_stderr = err[0];
        close(err[1]);
    }

    return res;
}

bool
process_terminate(pid_t pid) {
    if (pid <= 0) {
        LOGC("Requested to kill %d, this is an error. Please report the bug.\n",
             (int) pid);
        abort();
    }
    return kill(pid, SIGKILL) != -1;
}

exit_code_t
process_wait(pid_t pid, bool close) {
    int code;
    int options = WEXITED;
    if (!close) {
        options |= WNOWAIT;
    }

    siginfo_t info;
    int r = waitid(P_PID, pid, &info, options);
    if (r == -1 || info.si_code != CLD_EXITED) {
        // could not wait, or exited unexpectedly, probably by a signal
        code = NO_EXIT_CODE;
    } else {
        code = info.si_status;
    }
    return code;
}

void
process_close(pid_t pid) {
    process_wait(pid, true); // ignore exit code
}

ssize_t
read_pipe(int pipe, char *data, size_t len) {
    return read(pipe, data, len);
}

void
close_pipe(int pipe) {
    if (close(pipe)) {
        perror("close pipe");
    }
}
