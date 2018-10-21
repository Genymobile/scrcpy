#include "command.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "log.h"

enum process_result cmd_execute_redirect(const char *path, const char *const argv[], pid_t *pid,
                                         int *pipe_stdin, int *pipe_stdout, int *pipe_stderr) {
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
            execvp(path, (char *const *)argv);
            perror("exec");
            err = errno == ENOENT ? PROCESS_ERROR_MISSING_BINARY
                                  : PROCESS_ERROR_GENERIC;
        } else {
            perror("fcntl");
            err = PROCESS_ERROR_GENERIC;
        }
        printf("==> %d\n",(int)err);
        // send err to the parent
        if (write(internal[1], &err, sizeof(err)) == -1) {
            perror("write");
        }
        close(internal[1]);
        _exit(1);
    }

    /* parent */
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

enum process_result cmd_execute(const char *path, const char *const argv[], pid_t *pid) {
    return cmd_execute_redirect(path, argv, pid, NULL, NULL, NULL);
}

SDL_bool cmd_terminate(pid_t pid) {
    if (pid <= 0) {
        LOGC("Requested to kill %d, this is an error. Please report the bug.\n", (int) pid);
        abort();
    }
    return kill(pid, SIGTERM) != -1;
}

SDL_bool cmd_simple_wait(pid_t pid, int *exit_code) {
    int status;
    int code;
    if (waitpid(pid, &status, 0) == -1 || !WIFEXITED(status)) {
        // cannot wait, or exited unexpectedly, probably by a signal
        code = -1;
    } else {
        code = WEXITSTATUS(status);
    }
    if (exit_code) {
        *exit_code = code;
    }
    return !code;
}

int read_pipe(int pipe, char *data, size_t len) {
    return read(pipe, data, len);
}

void close_pipe(int pipe) {
    if (close(pipe)) {
        perror("close pipe");
    }
}
