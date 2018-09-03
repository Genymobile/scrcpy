#include "command.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "log.h"

int cmd_execute(const char *path, const char *const argv[], pid_t *pid) {
    int fd[2];
    int ret = 0;

    if (0 > pipe(fd)) {
        perror("pipe");
        return -1;
    }

    *pid = fork();
    if (*pid == -1) {
        perror("fork");
        ret = -1;
        goto END;
    }

    if (*pid > 0) {
        /* parent close write side. */
        close(fd[1]);
        fd[1] = -1;
        /* and blocking read until child exec or write some fail
         * reason in fd. */
        if (read(fd[0], &ret, sizeof(int)) == -1) {
            perror("read");
            ret = -1;
            goto END;
        }
    } else if (*pid == 0) {
        /* child close read side. */
        close(fd[0]);
        if (fcntl(fd[1], F_SETFD, FD_CLOEXEC) == -1) {
            perror("fcntl");
	    /* To prevent parent hang in read, we should exit when error. */
            close(fd[1]);
	    _exit(1);
        }
        execvp(path, (char *const *)argv);
        /* if child execvp failed, before exit, we should write
         * reason into the pipe. */
        ret = errno;
        if (write(fd[1], &ret, sizeof(int)) == -1) {
            perror("write");
        }
        /* close write side before exiting. */
        close(fd[1]);
        _exit(1);
    }

END:
    if (fd[0] != -1) {
        close(fd[0]);
    }
    if (fd[1] != -1) {
        close(fd[1]);
    }
    return ret;
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
