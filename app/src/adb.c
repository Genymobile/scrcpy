#include "adb.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adb_parser.h"
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
process_check_success_internal(sc_pid pid, const char *name, bool close,
                               unsigned flags) {
    bool log_errors = !(flags & SC_ADB_NO_LOGERR);

    if (pid == SC_PROCESS_NONE) {
        if (log_errors) {
            LOGE("Could not execute \"%s\"", name);
        }
        return false;
    }
    sc_exit_code exit_code = sc_process_wait(pid, close);
    if (exit_code) {
        if (log_errors) {
            if (exit_code != SC_EXIT_CODE_NONE) {
                LOGE("\"%s\" returned with value %" SC_PRIexitcode, name,
                     exit_code);
            } else {
                LOGE("\"%s\" exited unexpectedly", name);
            }
        }
        return false;
    }
    return true;
}

static bool
process_check_success_intr(struct sc_intr *intr, sc_pid pid, const char *name,
                           unsigned flags) {
    if (!sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return false;
    }

    // Always pass close=false, interrupting would be racy otherwise
    bool ret = process_check_success_internal(pid, name, false, flags);

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
              unsigned flags, sc_pipe *pout) {
    const char **argv = adb_create_argv(serial, adb_cmd, len);
    if (!argv) {
        return SC_PROCESS_NONE;
    }

    unsigned process_flags = 0;
    if (flags & SC_ADB_NO_STDOUT) {
        process_flags |= SC_PROCESS_NO_STDOUT;
    }
    if (flags & SC_ADB_NO_STDERR) {
        process_flags |= SC_PROCESS_NO_STDERR;
    }

    sc_pid pid;
    enum sc_process_result r =
        sc_process_execute_p(argv, &pid, process_flags, NULL, pout, NULL);
    if (r != SC_PROCESS_SUCCESS) {
        // If the execution itself failed (not the command exit code), log the
        // error in all cases
        show_adb_err_msg(r, argv);
        pid = SC_PROCESS_NONE;
    }

    free(argv);
    return pid;
}

sc_pid
adb_execute(const char *serial, const char *const adb_cmd[], size_t len,
            unsigned flags) {
    return adb_execute_p(serial, adb_cmd, len, flags, NULL);
}

bool
adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
            const char *device_socket_name, unsigned flags) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"forward", local, remote};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags);
    return process_check_success_intr(intr, pid, "adb forward", flags);
}

bool
adb_forward_remove(struct sc_intr *intr, const char *serial,
                   uint16_t local_port, unsigned flags) {
    char local[4 + 5 + 1]; // tcp:PORT
    sprintf(local, "tcp:%" PRIu16, local_port);
    const char *const adb_cmd[] = {"forward", "--remove", local};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags);
    return process_check_success_intr(intr, pid, "adb forward --remove", flags);
}

bool
adb_reverse(struct sc_intr *intr, const char *serial,
            const char *device_socket_name, uint16_t local_port,
            unsigned flags) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", remote, local};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags);
    return process_check_success_intr(intr, pid, "adb reverse", flags);
}

bool
adb_reverse_remove(struct sc_intr *intr, const char *serial,
                   const char *device_socket_name, unsigned flags) {
    char remote[108 + 14 + 1]; // localabstract:NAME
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", "--remove", remote};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags);
    return process_check_success_intr(intr, pid, "adb reverse --remove", flags);
}

bool
adb_push(struct sc_intr *intr, const char *serial, const char *local,
         const char *remote, unsigned flags) {
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
    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags);

#ifdef __WINDOWS__
    free((void *) remote);
    free((void *) local);
#endif

    return process_check_success_intr(intr, pid, "adb push", flags);
}

bool
adb_install(struct sc_intr *intr, const char *serial, const char *local,
            unsigned flags) {
#ifdef __WINDOWS__
    // Windows will parse the string, so the local name must be quoted
    // (see sys/win/command.c)
    local = sc_str_quote(local);
    if (!local) {
        return SC_PROCESS_NONE;
    }
#endif

    const char *const adb_cmd[] = {"install", "-r", local};
    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags);

#ifdef __WINDOWS__
    free((void *) local);
#endif

    return process_check_success_intr(intr, pid, "adb install", flags);
}

bool
adb_tcpip(struct sc_intr *intr, const char *serial, uint16_t port,
          unsigned flags) {
    char port_string[5 + 1];
    sprintf(port_string, "%" PRIu16, port);
    const char *const adb_cmd[] = {"tcpip", port_string};

    sc_pid pid = adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags);
    return process_check_success_intr(intr, pid, "adb tcpip", flags);
}

bool
adb_connect(struct sc_intr *intr, const char *ip_port, unsigned flags) {
    const char *const adb_cmd[] = {"connect", ip_port};

    sc_pipe pout;
    sc_pid pid = adb_execute_p(NULL, adb_cmd, ARRAY_LEN(adb_cmd), flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb connect\"");
        return false;
    }

    // "adb connect" always returns successfully (with exit code 0), even in
    // case of failure. As a workaround, check if its output starts with
    // "connected".
    char buf[128];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf));
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "adb connect", flags);
    if (!ok) {
        return false;
    }

    if (r == -1) {
        return false;
    }

    ok = !strncmp("connected", buf, sizeof("connected") - 1);
    if (!ok && !(flags & SC_ADB_NO_STDERR)) {
        // "adb connect" also prints errors to stdout. Since we capture it,
        // re-print the error to stderr.
        sc_str_truncate(buf, r, "\r\n");
        fprintf(stderr, "%s\n", buf);
    }
    return ok;
}

bool
adb_disconnect(struct sc_intr *intr, const char *ip_port, unsigned flags) {
    const char *const adb_cmd[] = {"disconnect", ip_port};
    size_t len = ip_port ? ARRAY_LEN(adb_cmd)
                         : ARRAY_LEN(adb_cmd) - 1;

    sc_pid pid = adb_execute(NULL, adb_cmd, len, flags);
    return process_check_success_intr(intr, pid, "adb disconnect", flags);
}

char *
adb_getprop(struct sc_intr *intr, const char *serial, const char *prop,
            unsigned flags) {
    const char *const adb_cmd[] = {"shell", "getprop", prop};

    sc_pipe pout;
    sc_pid pid =
        adb_execute_p(serial, adb_cmd, ARRAY_LEN(adb_cmd), flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb getprop\"");
        return NULL;
    }

    char buf[128];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf));
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "adb getprop", flags);
    if (!ok) {
        return NULL;
    }

    if (r == -1) {
        return NULL;
    }

    sc_str_truncate(buf, r, " \r\n");

    return strdup(buf);
}

char *
adb_get_serialno(struct sc_intr *intr, unsigned flags) {
    const char *const adb_cmd[] = {"get-serialno"};

    sc_pipe pout;
    sc_pid pid = adb_execute_p(NULL, adb_cmd, ARRAY_LEN(adb_cmd), flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb get-serialno\"");
        return NULL;
    }

    char buf[128];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf));
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "adb get-serialno", flags);
    if (!ok) {
        return NULL;
    }

    if (r == -1) {
        return false;
    }

    sc_str_truncate(buf, r, " \r\n");

    return strdup(buf);
}

char *
adb_get_device_ip(struct sc_intr *intr, const char *serial, unsigned flags) {
    const char *const cmd[] = {"shell", "ip", "route"};

    sc_pipe pout;
    sc_pid pid = adb_execute_p(serial, cmd, ARRAY_LEN(cmd), flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGD("Could not execute \"ip route\"");
        return NULL;
    }

    // "adb shell ip route" output should contain only a few lines
    char buf[1024];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf));
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "ip route", flags);
    if (!ok) {
        return NULL;
    }

    if (r == -1) {
        return false;
    }

    assert((size_t) r <= sizeof(buf));
    if (r == sizeof(buf) && buf[sizeof(buf) - 1] != '\0')  {
        // The implementation assumes that the output of "ip route" fits in the
        // buffer in a single pass
        LOGW("Result of \"ip route\" does not fit in 1Kb. "
             "Please report an issue.\n");
        return NULL;
    }

    return sc_adb_parse_device_ip_from_output(buf, r);
}
