#include "command.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t cmd_execute(const char *path, const char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execvp(path, (char *const *)argv);
        perror("exec");
        _exit(1);
    }
    return pid;
}

pid_t cmd_execute_redirect(const char *path, const char *const argv[],
                           int *pipe_stdin, int *pipe_stdout, int *pipe_stderr) {
    int in[2];
    int out[2];
    int err[2];
    if (pipe_stdin) {
        if (pipe(in) == -1) {
            perror("pipe");
            return -1;
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
            return -1;
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
            return -1;
        }
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
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
        execvp(path, (char *const *)argv);
        perror("exec");
        _exit(1);
    }

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

    return pid;
}

SDL_bool cmd_terminate(pid_t pid) {
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
