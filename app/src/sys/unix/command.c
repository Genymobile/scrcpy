// for portability
#define _POSIX_SOURCE // for kill()
#define _BSD_SOURCE // for readlink()

// modern glibc will complain without this
#define _DEFAULT_SOURCE

#include "command.h"

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util/log.h"

enum process_result
cmd_execute(const char *const argv[], pid_t *pid) {
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
            execvp(argv[0], (char *const *)argv);
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
        // could not wait, or exited unexpectedly, probably by a signal
        code = -1;
    } else {
        code = WEXITSTATUS(status);
    }
    if (exit_code) {
        *exit_code = code;
    }
    return !code;
}

char *
get_executable_path(void) {
// <https://stackoverflow.com/a/1024937/1987178>
#ifdef __linux__
    char buf[PATH_MAX + 1]; // +1 for the null byte
    ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
    if (len == -1) {
        perror("readlink");
        return NULL;
    }
    buf[len] = '\0';
    return SDL_strdup(buf);
#else
    // in practice, we only need this feature for portable builds, only used on
    // Windows, so we don't care implementing it for every platform
    // (it's useful to have a working version on Linux for debugging though)
    return NULL;
#endif
}
