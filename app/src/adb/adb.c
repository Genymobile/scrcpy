#include "adb.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "adb/adb_device.h"
#include "adb/adb_parser.h"
#include "util/env.h"
#include "util/file.h"
#include "util/log.h"
#include "util/process_intr.h"
#include "util/str.h"

/* Convenience macro to expand:
 *
 *     const char *const argv[] =
 *         SC_ADB_COMMAND("shell", "echo", "hello");
 *
 * to:
 *
 *     const char *const argv[] =
 *         { sc_adb_get_executable(), "shell", "echo", "hello", NULL };
 */
#define SC_ADB_COMMAND(...) { sc_adb_get_executable(), __VA_ARGS__, NULL }

static char *adb_executable;

bool
sc_adb_init(void) {
    adb_executable = sc_get_env("ADB");
    if (adb_executable) {
        LOGD("Using adb: %s", adb_executable);
        return true;
    }

#if !defined(PORTABLE) || defined(_WIN32)
    adb_executable = strdup("adb");
    if (!adb_executable) {
        LOG_OOM();
        return false;
    }
#else
    // For portable builds, use the absolute path to the adb executable
    // in the same directory as scrcpy (except on Windows, where "adb"
    // is sufficient)
    adb_executable = sc_file_get_local_path("adb");
    if (!adb_executable) {
        // Error already logged
        return false;
    }

    LOGD("Using adb (portable): %s", adb_executable);
#endif

    return true;
}

void
sc_adb_destroy(void) {
    free(adb_executable);
}

const char *
sc_adb_get_executable(void) {
    return adb_executable;
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
show_adb_installation_msg(void) {
#ifndef _WIN32
    static const struct {
        const char *binary;
        const char *command;
    } pkg_managers[] = {
        {"apt", "apt install adb"},
        {"apt-get", "apt-get install adb"},
        {"brew", "brew install --cask android-platform-tools"},
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
    if (intr && !sc_intr_set_process(intr, pid)) {
        // Already interrupted
        return false;
    }

    // Always pass close=false, interrupting would be racy otherwise
    bool ret = process_check_success_internal(pid, name, false, flags);

    if (intr) {
        sc_intr_set_process(intr, SC_PROCESS_NONE);
    }

    // Close separately
    sc_process_close(pid);

    return ret;
}

static sc_pid
sc_adb_execute_p(const char *const argv[], unsigned flags, sc_pipe *pout) {
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

    return pid;
}

sc_pid
sc_adb_execute(const char *const argv[], unsigned flags) {
    return sc_adb_execute_p(argv, flags, NULL);
}

bool
sc_adb_start_server(struct sc_intr *intr, unsigned flags) {
    const char *const argv[] = SC_ADB_COMMAND("start-server");

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb start-server", flags);
}

bool
sc_adb_kill_server(struct sc_intr *intr, unsigned flags) {
    const char *const argv[] = SC_ADB_COMMAND("kill-server");

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb kill-server", flags);
}

bool
sc_adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
               const char *device_socket_name, unsigned flags) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME

    int r = snprintf(local, sizeof(local), "tcp:%" PRIu16, local_port);
    assert(r >= 0 && (size_t) r < sizeof(local));

    r = snprintf(remote, sizeof(remote), "localabstract:%s",
                 device_socket_name);
    if (r < 0 || (size_t) r >= sizeof(remote)) {
        LOGE("Could not write socket name");
        return false;
    }

    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "forward", local, remote);

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb forward", flags);
}

bool
sc_adb_forward_remove(struct sc_intr *intr, const char *serial,
                      uint16_t local_port, unsigned flags) {
    char local[4 + 5 + 1]; // tcp:PORT
    int r = snprintf(local, sizeof(local), "tcp:%" PRIu16, local_port);
    assert(r >= 0 && (size_t) r < sizeof(local));
    (void) r;

    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "forward", "--remove", local);

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb forward --remove", flags);
}

bool
sc_adb_reverse(struct sc_intr *intr, const char *serial,
               const char *device_socket_name, uint16_t local_port,
               unsigned flags) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    int r = snprintf(local, sizeof(local), "tcp:%" PRIu16, local_port);
    assert(r >= 0 && (size_t) r < sizeof(local));

    r = snprintf(remote, sizeof(remote), "localabstract:%s",
                 device_socket_name);
    if (r < 0 || (size_t) r >= sizeof(remote)) {
        LOGE("Could not write socket name");
        return false;
    }

    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "reverse", remote, local);

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb reverse", flags);
}

bool
sc_adb_reverse_remove(struct sc_intr *intr, const char *serial,
                      const char *device_socket_name, unsigned flags) {
    char remote[108 + 14 + 1]; // localabstract:NAME
    int r = snprintf(remote, sizeof(remote), "localabstract:%s",
                     device_socket_name);
    if (r < 0 || (size_t) r >= sizeof(remote)) {
        LOGE("Device socket name too long");
        return false;
    }

    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "reverse", "--remove", remote);

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb reverse --remove", flags);
}

bool
sc_adb_push(struct sc_intr *intr, const char *serial, const char *local,
            const char *remote, unsigned flags) {
#ifdef _WIN32
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

    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "push", local, remote);

    sc_pid pid = sc_adb_execute(argv, flags);

#ifdef _WIN32
    free((void *) remote);
    free((void *) local);
#endif

    return process_check_success_intr(intr, pid, "adb push", flags);
}

bool
sc_adb_install(struct sc_intr *intr, const char *serial, const char *local,
               unsigned flags) {
#ifdef _WIN32
    // Windows will parse the string, so the local name must be quoted
    // (see sys/win/command.c)
    local = sc_str_quote(local);
    if (!local) {
        return SC_PROCESS_NONE;
    }
#endif

    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "install", "-r", local);

    sc_pid pid = sc_adb_execute(argv, flags);

#ifdef _WIN32
    free((void *) local);
#endif

    return process_check_success_intr(intr, pid, "adb install", flags);
}

bool
sc_adb_tcpip(struct sc_intr *intr, const char *serial, uint16_t port,
             unsigned flags) {
    char port_string[5 + 1];
    int r = snprintf(port_string, sizeof(port_string), "%" PRIu16, port);
    assert(r >= 0 && (size_t) r < sizeof(port_string));
    (void) r;

    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "tcpip", port_string);

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb tcpip", flags);
}

bool
sc_adb_connect(struct sc_intr *intr, const char *ip_port, unsigned flags) {
    const char *const argv[] = SC_ADB_COMMAND("connect", ip_port);

    sc_pipe pout;
    sc_pid pid = sc_adb_execute_p(argv, flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb connect\"");
        return false;
    }

    // "adb connect" always returns successfully (with exit code 0), even in
    // case of failure. As a workaround, check if its output starts with
    // "connected" or "already connected".
    char buf[128];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf) - 1);
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "adb connect", flags);
    if (!ok) {
        return false;
    }

    if (r == -1) {
        return false;
    }

    assert((size_t) r < sizeof(buf));
    buf[r] = '\0';

    ok = !strncmp("connected", buf, sizeof("connected") - 1)
        || !strncmp("already connected", buf, sizeof("already connected") - 1);
    if (!ok && !(flags & SC_ADB_NO_STDERR)) {
        // "adb connect" also prints errors to stdout. Since we capture it,
        // re-print the error to stderr.
        size_t len = strcspn(buf, "\r\n");
        buf[len] = '\0';
        fprintf(stderr, "%s\n", buf);
    }
    return ok;
}

bool
sc_adb_disconnect(struct sc_intr *intr, const char *ip_port, unsigned flags) {
    assert(ip_port);
    const char *const argv[] = SC_ADB_COMMAND("disconnect", ip_port);

    sc_pid pid = sc_adb_execute(argv, flags);
    return process_check_success_intr(intr, pid, "adb disconnect", flags);
}

static bool
sc_adb_list_devices(struct sc_intr *intr, unsigned flags,
                    struct sc_vec_adb_devices *out_vec) {
    const char *const argv[] = SC_ADB_COMMAND("devices", "-l");

#define BUFSIZE 65536
    char *buf = malloc(BUFSIZE);
    if (!buf) {
        LOG_OOM();
        return false;
    }

    sc_pipe pout;
    sc_pid pid = sc_adb_execute_p(argv, flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb devices -l\"");
        free(buf);
        return false;
    }

    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, BUFSIZE - 1);
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "adb devices -l", flags);
    if (!ok) {
        free(buf);
        return false;
    }

    if (r == -1) {
        free(buf);
        return false;
    }

    assert((size_t) r < BUFSIZE);
    if (r == BUFSIZE - 1)  {
        // The implementation assumes that the output of "adb devices -l" fits
        // in the buffer in a single pass
        LOGW("Result of \"adb devices -l\" does not fit in 64Kb. "
             "Please report an issue.");
        free(buf);
        return false;
    }

    // It is parsed as a NUL-terminated string
    buf[r] = '\0';

    // List all devices to the output list directly
    ok = sc_adb_parse_devices(buf, out_vec);
    free(buf);
    return ok;
}

static bool
sc_adb_accept_device(const struct sc_adb_device *device,
                     const struct sc_adb_device_selector *selector) {
    switch (selector->type) {
        case SC_ADB_DEVICE_SELECT_ALL:
            return true;
        case SC_ADB_DEVICE_SELECT_SERIAL:
            assert(selector->serial);
            char *device_serial_colon = strchr(device->serial, ':');
            if (device_serial_colon) {
                // The device serial is an IP:port...
                char *serial_colon = strchr(selector->serial, ':');
                if (!serial_colon) {
                    // But the requested serial has no ':', so only consider
                    // the IP part of the device serial. This allows to use
                    // "192.168.1.1" to match any "192.168.1.1:port".
                    size_t serial_len = strlen(selector->serial);
                    size_t device_ip_len = device_serial_colon - device->serial;
                    if (serial_len != device_ip_len) {
                        // They are not equal, they don't even have the same
                        // length
                        return false;
                    }
                    return !strncmp(selector->serial, device->serial,
                                    device_ip_len);
                }
            }
            return !strcmp(selector->serial, device->serial);
        case SC_ADB_DEVICE_SELECT_USB:
            return sc_adb_device_get_type(device->serial) ==
                    SC_ADB_DEVICE_TYPE_USB;
        case SC_ADB_DEVICE_SELECT_TCPIP:
            // Both emulators and TCP/IP devices are selected via -e
            return sc_adb_device_get_type(device->serial) !=
                    SC_ADB_DEVICE_TYPE_USB;
        default:
            assert(!"Missing SC_ADB_DEVICE_SELECT_* handling");
            break;
    }

    return false;
}

static size_t
sc_adb_devices_select(struct sc_adb_device *devices, size_t len,
                      const struct sc_adb_device_selector *selector,
                      size_t *idx_out) {
    size_t count = 0;
    for (size_t i = 0; i < len; ++i) {
        struct sc_adb_device *device = &devices[i];
        device->selected = sc_adb_accept_device(device, selector);
        if (device->selected) {
            if (idx_out && !count) {
                *idx_out = i;
            }
            ++count;
        }
    }

    return count;
}

static void
sc_adb_devices_log(enum sc_log_level level, struct sc_adb_device *devices,
                   size_t count) {
    for (size_t i = 0; i < count; ++i) {
        struct sc_adb_device *d = &devices[i];
        const char *selection = d->selected ? "-->" : "   ";
        bool is_usb =
            sc_adb_device_get_type(d->serial) == SC_ADB_DEVICE_TYPE_USB;
        const char *type = is_usb ? "  (usb)"
                                  : "(tcpip)";
        LOG(level, "    %s %s  %-20s  %16s  %s",
             selection, type, d->serial, d->state, d->model ? d->model : "");
    }
}

static bool
sc_adb_device_check_state(struct sc_adb_device *device,
                          struct sc_adb_device *devices, size_t count) {
    const char *state = device->state;

    if (!strcmp("device", state)) {
        return true;
    }

    if (!strcmp("unauthorized", state)) {
        LOGE("Device is unauthorized:");
        sc_adb_devices_log(SC_LOG_LEVEL_ERROR, devices, count);
        LOGE("A popup should open on the device to request authorization.");
        LOGE("Check the FAQ: "
             "<https://github.com/Genymobile/scrcpy/blob/master/FAQ.md>");
    } else {
        LOGE("Device could not be connected (state=%s)", state);
    }

    return false;
}

bool
sc_adb_select_device(struct sc_intr *intr,
                     const struct sc_adb_device_selector *selector,
                     unsigned flags, struct sc_adb_device *out_device) {
    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_list_devices(intr, flags, &vec);
    if (!ok) {
        LOGE("Could not list ADB devices");
        return false;
    }

    if (vec.size == 0) {
        LOGE("Could not find any ADB device");
        return false;
    }

    size_t sel_idx; // index of the single matching device if sel_count == 1
    size_t sel_count =
        sc_adb_devices_select(vec.data, vec.size, selector, &sel_idx);

    if (sel_count == 0) {
        // if count > 0 && sel_count == 0, then necessarily a selection is
        // requested
        assert(selector->type != SC_ADB_DEVICE_SELECT_ALL);

        switch (selector->type) {
            case SC_ADB_DEVICE_SELECT_SERIAL:
                assert(selector->serial);
                LOGE("Could not find ADB device %s:", selector->serial);
                break;
            case SC_ADB_DEVICE_SELECT_USB:
                LOGE("Could not find any ADB device over USB:");
                break;
            case SC_ADB_DEVICE_SELECT_TCPIP:
                LOGE("Could not find any ADB device over TCP/IP:");
                break;
            default:
                assert(!"Unexpected selector type");
                break;
        }

        sc_adb_devices_log(SC_LOG_LEVEL_ERROR, vec.data, vec.size);
        sc_adb_devices_destroy(&vec);
        return false;
    }

    if (sel_count > 1) {
        switch (selector->type) {
            case SC_ADB_DEVICE_SELECT_ALL:
                LOGE("Multiple (%" SC_PRIsizet ") ADB devices:", sel_count);
                break;
            case SC_ADB_DEVICE_SELECT_SERIAL:
                assert(selector->serial);
                LOGE("Multiple (%" SC_PRIsizet ") ADB devices with serial %s:",
                     sel_count, selector->serial);
                break;
            case SC_ADB_DEVICE_SELECT_USB:
                LOGE("Multiple (%" SC_PRIsizet ") ADB devices over USB:",
                     sel_count);
                break;
            case SC_ADB_DEVICE_SELECT_TCPIP:
                LOGE("Multiple (%" SC_PRIsizet ") ADB devices over TCP/IP:",
                     sel_count);
                break;
            default:
                assert(!"Unexpected selector type");
                break;
        }
        sc_adb_devices_log(SC_LOG_LEVEL_ERROR, vec.data, vec.size);
        LOGE("Select a device via -s (--serial), -d (--select-usb) or -e "
             "(--select-tcpip)");
        sc_adb_devices_destroy(&vec);
        return false;
    }

    assert(sel_count == 1); // sel_idx is valid only if sel_count == 1
    struct sc_adb_device *device = &vec.data[sel_idx];

    ok = sc_adb_device_check_state(device, vec.data, vec.size);
    if (!ok) {
        sc_adb_devices_destroy(&vec);
        return false;
    }

    LOGI("ADB device found:");
    sc_adb_devices_log(SC_LOG_LEVEL_INFO, vec.data, vec.size);

    // Move devics into out_device (do not destroy device)
    sc_adb_device_move(out_device, device);
    sc_adb_devices_destroy(&vec);
    return true;
}

char *
sc_adb_getprop(struct sc_intr *intr, const char *serial, const char *prop,
               unsigned flags) {
    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "shell", "getprop", prop);

    sc_pipe pout;
    sc_pid pid = sc_adb_execute_p(argv, flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGE("Could not execute \"adb getprop\"");
        return NULL;
    }

    char buf[128];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf) - 1);
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "adb getprop", flags);
    if (!ok) {
        return NULL;
    }

    if (r == -1) {
        return NULL;
    }

    assert((size_t) r < sizeof(buf));
    buf[r] = '\0';
    size_t len = strcspn(buf, " \r\n");
    buf[len] = '\0';

    return strdup(buf);
}

char *
sc_adb_get_device_ip(struct sc_intr *intr, const char *serial, unsigned flags) {
    assert(serial);
    const char *const argv[] =
        SC_ADB_COMMAND("-s", serial, "shell", "ip", "route");

    sc_pipe pout;
    sc_pid pid = sc_adb_execute_p(argv, flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        LOGD("Could not execute \"ip route\"");
        return NULL;
    }

    // "adb shell ip route" output should contain only a few lines
    char buf[1024];
    ssize_t r = sc_pipe_read_all_intr(intr, pid, pout, buf, sizeof(buf) - 1);
    sc_pipe_close(pout);

    bool ok = process_check_success_intr(intr, pid, "ip route", flags);
    if (!ok) {
        return NULL;
    }

    if (r == -1) {
        return NULL;
    }

    assert((size_t) r < sizeof(buf));
    if (r == sizeof(buf) - 1)  {
        // The implementation assumes that the output of "ip route" fits in the
        // buffer in a single pass
        LOGW("Result of \"ip route\" does not fit in 1Kb. "
             "Please report an issue.");
        return NULL;
    }

    // It is parsed as a NUL-terminated string
    buf[r] = '\0';

    return sc_adb_parse_device_ip(buf);
}

uint16_t
sc_adb_get_device_sdk_version(struct sc_intr *intr, const char *serial) {
    char *sdk_version =
        sc_adb_getprop(intr, serial, "ro.build.version.sdk", SC_ADB_SILENT);
    if (!sdk_version) {
        return 0;
    }

    long value;
    bool ok = sc_str_parse_integer(sdk_version, &value);
    free(sdk_version);
    if (!ok || value < 0 || value > 0xFFFF) {
        return 0;
    }

    return value;
}
