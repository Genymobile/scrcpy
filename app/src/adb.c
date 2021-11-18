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
        LOGE("Failed to execute (could not allocate error message)");
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

static sc_pid
adb_execute_p(const char *serial, const char *const adb_cmd[],
              size_t len, sc_pipe *pout) {
    int i;
    sc_pid pid;

    const char **argv = malloc((len + 4) * sizeof(*argv));
    if (!argv) {
        return SC_PROCESS_NONE;
    }

    argv[0] = get_adb_command();
    if (serial) {
        argv[1] = "-s";
        argv[2] = serial;
        i = 3;
    } else {
        i = 1;
    }

    memcpy(&argv[i], adb_cmd, len * sizeof(const char *));
    argv[len + i] = NULL;
    enum sc_process_result r =
        sc_process_execute_p(argv, &pid, NULL, pout, NULL);
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

static sc_pid
adb_exec_forward(const char *serial, uint16_t local_port,
                 const char *device_socket_name) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"forward", local, remote};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

static sc_pid
adb_exec_forward_remove(const char *serial, uint16_t local_port) {
    char local[4 + 5 + 1]; // tcp:PORT
    sprintf(local, "tcp:%" PRIu16, local_port);
    const char *const adb_cmd[] = {"forward", "--remove", local};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

static sc_pid
adb_exec_reverse(const char *serial, const char *device_socket_name,
                 uint16_t local_port) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", remote, local};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

static sc_pid
adb_exec_reverse_remove(const char *serial, const char *device_socket_name) {
    char remote[108 + 14 + 1]; // localabstract:NAME
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", "--remove", remote};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

static sc_pid
adb_exec_push(const char *serial, const char *local, const char *remote) {
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

    return pid;
}

static sc_pid
adb_exec_install(const char *serial, const char *local) {
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

    return pid;
}

static sc_pid
adb_exec_get_serialno(sc_pipe *pout) {
    const char *const adb_cmd[] = {"get-serialno"};
    return adb_execute_p(NULL, adb_cmd, ARRAY_LEN(adb_cmd), pout);
}

bool
adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
            const char *device_socket_name) {
    sc_pid pid = adb_exec_forward(serial, local_port, device_socket_name);
    return sc_process_check_success_intr(intr, pid, "adb forward", true);
}

bool
adb_forward_remove(struct sc_intr *intr, const char *serial,
                   uint16_t local_port) {
    sc_pid pid = adb_exec_forward_remove(serial, local_port);
    return sc_process_check_success_intr(intr, pid, "adb forward --remove",
                                         true);
}

bool
adb_reverse(struct sc_intr *intr, const char *serial,
            const char *device_socket_name, uint16_t local_port) {
    sc_pid pid = adb_exec_reverse(serial, device_socket_name, local_port);
    return sc_process_check_success_intr(intr, pid, "adb reverse", true);
}

bool
adb_reverse_remove(struct sc_intr *intr, const char *serial,
                   const char *device_socket_name) {
    sc_pid pid = adb_exec_reverse_remove(serial, device_socket_name);
    return sc_process_check_success_intr(intr, pid, "adb reverse --remove",
                                         true);
}

bool
adb_push(struct sc_intr *intr, const char *serial, const char *local,
         const char *remote) {
    sc_pid pid = adb_exec_push(serial, local, remote);
    return sc_process_check_success_intr(intr, pid, "adb push", true);
}

bool
adb_install(struct sc_intr *intr, const char *serial, const char *local) {
    sc_pid pid = adb_exec_install(serial, local);
    return sc_process_check_success_intr(intr, pid, "adb install", true);
}

char *
adb_get_serialno(struct sc_intr *intr) {
    sc_pipe pout;
    sc_pid pid = adb_exec_get_serialno(&pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb get-serialno\"");
        return NULL;
    }

    char buf[128];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf));
    sc_pipe_close(pout);

    bool ok =
        sc_process_check_success_intr(intr, pid, "adb get-serialno", true);
    if (!ok) {
        return NULL;
    }

    sc_str_truncate(buf, r, " \r\n");

    return strdup(buf);
}
