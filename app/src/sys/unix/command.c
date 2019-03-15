#define _POSIX_SOURCE // for kill()

#include "command.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "log.h"

enum process_result
cmd_execute(const char *path, const char *const argv[], pid_t *pid) {
    int fd[2];

    if (pipe(fd) == -1) {
        perror("pipe");
        return PROCESS_ERROR_GENERIC;
    }

    enum process_result ret = PROCESS_SUCCESS;

    *pid = fork();
    if (*pid == -1) {
        perror("fork");
        ret = PROCESS_ERROR_GENERIC;
        goto end;
    }

    if (*pid > 0) {
        // parent close write side
        close(fd[1]);
        fd[1] = -1;
        // wait for EOF or receive errno from child
        if (read(fd[0], &ret, sizeof(ret)) == -1) {
            perror("read");
            ret = PROCESS_ERROR_GENERIC;
            goto end;
        }
    } else if (*pid == 0) {
        // child close read side
        close(fd[0]);
        if (fcntl(fd[1], F_SETFD, FD_CLOEXEC) == 0) {
            execvp(path, (char *const *)argv);
            if (errno == ENOENT) {
                ret = PROCESS_ERROR_MISSING_BINARY;
            } else {
                ret = PROCESS_ERROR_GENERIC;
            }
            perror("exec");
        } else {
            perror("fcntl");
            ret = PROCESS_ERROR_GENERIC;
        }
        // send ret to the parent
        if (write(fd[1], &ret, sizeof(ret)) == -1) {
            perror("write");
        }
        // close write side before exiting
        close(fd[1]);
        _exit(1);
    }

end:
    if (fd[0] != -1) {
        close(fd[0]);
    }
    if (fd[1] != -1) {
        close(fd[1]);
    }
    return ret;
}

bool
cmd_terminate(pid_t pid) {
    if (pid <= 0) {
        LOGC("Requested to kill %d, this is an error. Please report the bug.\n",
             (int) pid);
        abort();
    }
    return kill(pid, SIGTERM) != -1;
}

bool
cmd_simple_wait(pid_t pid, int *exit_code) {
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
