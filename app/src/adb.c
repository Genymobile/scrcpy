#include "adb.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/file.h"
#include "util/log.h"
#include "util/process_intr.h"
#include "util/str.h"

static const char *adb_command;

static inline const char *
get_adb_command(void) {
    if (!adb_command) {
        adb_command = getenv("ADB");
        if (!adb_command)
            adb_command = "adb";
    }
    return adb_command;
}

// serialize argv to string "[arg1], [arg2], [arg3]"
static size_t
argv_to_string(const char *const *argv, char *buf, size_t bufsize) {
    size_t idx = 0;
    bool first = true;
    while (*argv) {
        const char *arg = *argv;
        size_t len = strlen(arg);
        // count space for "[], ...\0"
        if (idx + len + 8 >= bufsize) {
            // not enough space, truncate
            assert(idx < bufsize - 4);
            memcpy(&buf[idx], "...", 3);
            idx += 3;
            break;
        }
        if (first) {
            first = false;
        } else {
            buf[idx++] = ',';
            buf[idx++] = ' ';
        }
        buf[idx++] = '[';
        memcpy(&buf[idx], arg, len);
        idx += len;
        buf[idx++] = ']';
        argv++;
    }
    assert(idx < bufsize);
    buf[idx] = '\0';
    return idx;
}

static void
show_adb_installation_msg() {
#ifndef __WINDOWS__
    static const struct {
        const char *binary;
        const char *command;
    } pkg_managers[] = {
        {"apt", "apt install adb"},
        {"apt-get", "apt-get install adb"},
        {"brew", "brew cask install android-platform-tools"},
        {"dnf", "dnf install android-tools"},
        {"emerge", "emerge dev-util/android-tools"},
        {"pacman", "pacman -S android-tools"},
    };
    for (size_t i = 0; i < ARRAY_LEN(pkg_managers); ++i) {
        if (sc_file_executable_exists(pkg_managers[i].binary)) {
            LOGI("You may install 'adb' by \"%s\"", pkg_managers[i].command);
            return;
        }
    }
#endif

    LOGI("You may download and install 'adb' from "
         "https://developer.android.com/studio/releases/platform-tools");
}

static void
show_adb_err_msg(enum sc_process_result err, const char *const argv[]) {
#define MAX_COMMAND_STRING_LEN 1024
    char *buf = malloc(MAX_COMMAND_STRING_LEN);
    if (!buf) {
        LOG_OOM();
        LOGE("Failed to execute");
        return;
    }

    switch (err) {
        case SC_PROCESS_ERROR_GENERIC:
            argv_to_string(argv, buf, MAX_COMMAND_STRING_LEN);
            LOGE("Failed to execute: %s", buf);
            break;
        case SC_PROCESS_ERROR_MISSING_BINARY:
            argv_to_string(argv, buf, MAX_COMMAND_STRING_LEN);
            LOGE("Command not found: %s", buf);
            LOGE("(make 'adb' accessible from your PATH or define its full"
                 "path in the ADB environment variable)");
            show_adb_installation_msg();
            break;
        case SC_PROCESS_SUCCESS:
            // do nothing
            break;
    }

    free(buf);
}

static bool
process_check_success_internal(sc_pid pid, const char *name, bool close) {
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

static bool
process_check_success_intr(struct sc_intr *intr, sc_pid pid, const char *name) {
    if (!sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return false;
    }

    // Always pass close=false, interrupting would be racy otherwise
    bool ret = process_check_success_internal(pid, name, false);

    sc_intr_set_process(intr, SC_PROCESS_NONE);

    // Close separately
    sc_process_close(pid);

    return ret;
}

static const char **
adb_create_argv(const char *serial, const char *const adb_cmd[], size_t len) {
    const char **argv = malloc((len + 4) * sizeof(*argv));
    if (!argv) {
        LOG_OOM();
        return NULL;
    }

    argv[0] = get_adb_command();
    int i;
    if (serial) {
        argv[1] = "-s";
        argv[2] = serial;
        i = 3;
    } else {
        i = 1;
    }

    memcpy(&argv[i], adb_cmd, len * sizeof(const char *));
    argv[len + i] = NULL;
    return argv;
}

static sc_pid
adb_execute_p(const char *serial, const char *const adb_cmd[], size_t len,
              sc_pipe *pout) {
    const char **argv = adb_create_argv(serial, adb_cmd, len);
    if (!argv) {
        return SC_PROCESS_NONE;
    }

    sc_pid pid;
    enum sc_process_result r =
        sc_process_execute_p(argv, &pid, 0, NULL, pout, NULL);
    if (r != SC_PROCESS_SUCCESS) {
        show_adb_err_msg(r, argv);
        pid = SC_PROCESS_NONE;
    }

    free(argv);
    return pid;
}

sc_pid
adb_execute(const char *serial, const char *const adb_cmd[], size_t len) {
    return adb_execute_p(serial, adb_cmd, len, NULL);
}

bool
adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
            const char *device_socket_name) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"forward", local, remote};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
    return process_check_success_intr(intr, pid, "adb forward");
}

bool
adb_forward_remove(struct sc_intr *intr, const char *serial,
                   uint16_t local_port) {
    char local[4 + 5 + 1]; // tcp:PORT
    sprintf(local, "tcp:%" PRIu16, local_port);
    const char *const adb_cmd[] = {"forward", "--remove", local};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
    return process_check_success_intr(intr, pid, "adb forward --remove");
}

bool
adb_reverse(struct sc_intr *intr, const char *serial,
            const char *device_socket_name, uint16_t local_port) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", remote, local};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
    return process_check_success_intr(intr, pid, "adb reverse");
}

bool
adb_reverse_remove(struct sc_intr *intr, const char *serial,
                   const char *device_socket_name) {
    char remote[108 + 14 + 1]; // localabstract:NAME
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", "--remove", remote};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
    return process_check_success_intr(intr, pid, "adb reverse --remove");
}

bool
adb_push(struct sc_intr *intr, const char *serial, const char *local,
         const char *remote) {
#ifdef __WINDOWS__
    // Windows will parse the string, so the paths must be quoted
    // (see sys/win/command.c)
    local = sc_str_quote(local);
    if (!local) {
        return SC_PROCESS_NONE;
    }
    remote = sc_str_quote(remote);
    if (!remote) {
        free((void *) local);
        return SC_PROCESS_NONE;
    }
#endif

    const char *const adb_cmd[] = {"push", local, remote};
    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));

#ifdef __WINDOWS__
    free((void *) remote);
    free((void *) local);
#endif

    return process_check_success_intr(intr, pid, "adb push");
}

bool
adb_install(struct sc_intr *intr, const char *serial, const char *local) {
#ifdef __WINDOWS__
    // Windows will parse the string, so the local name must be quoted
    // (see sys/win/command.c)
    local = sc_str_quote(local);
    if (!local) {
        return SC_PROCESS_NONE;
    }
#endif

    const char *const adb_cmd[] = {"install", "-r", local};
    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));

#ifdef __WINDOWS__
    free((void *) local);
#endif

    return process_check_success_intr(intr, pid, "adb install");
}

char *
adb_get_serialno(struct sc_intr *intr) {
    const char *const adb_cmd[] = {"get-serialno"};

    sc_pipe pout;
    sc_pid pid = adb_execute_p(NULL, adb_cmd, ARRAY_LEN(adb_cmd), &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb get-serialno\"");
        return NULL;
    }

    char buf[128];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf));
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "adb get-serialno");
    if (!ok) {
        return NULL;
    }

    if (r == -1) {
        return false;
    }

    sc_str_truncate(buf, r, " \r\n");

    return strdup(buf);
}
