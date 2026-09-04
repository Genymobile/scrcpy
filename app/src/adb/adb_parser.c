#include "adb_parser.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "util/log.h"
#include "util/str.h"

static size_t
rstrip_len(const char *s, size_t len) {
    size_t i = len;

    // Ignore trailing whitespaces
    while (i > 0 && (s[i-1] == ' ' || s[i-1] == '\t')) {
        --i;
    }

    return i;
}

static void
locate_last_token(const char *s, size_t len, size_t *start, size_t *end) {
    size_t i = rstrip_len(s, len);
    *end = i; // excluded

    // The token contains non-whitespace chars
    while (i > 0 && (s[i-1] != ' ' && s[i-1] != '\t')) {
        --i;
    }

    *start = i; // included
}

static bool
is_device_state(const char *s) {
    // <https://android.googlesource.com/platform/packages/modules/adb/+/1cf2f017d312f73b3dc53bda85ef2610e35a80e9/adb.cpp#144>
    // "device", "unauthorized" and "offline" are the most common states, so
    // check them first.
    return !strcmp(s, "device")
        || !strcmp(s, "unauthorized")
        || !strcmp(s, "offline")
        || !strcmp(s, "bootloader")
        || !strcmp(s, "host")
        || !strcmp(s, "recovery")
        || !strcmp(s, "rescue")
        || !strcmp(s, "sideload")
        || !strcmp(s, "authorizing")
        || !strcmp(s, "connecting")
        || !strcmp(s, "detached");
}

static bool
sc_adb_parse_device(char *line, struct sc_adb_device *device) {
    // One device line looks like:
    // "0123456789abcdef	device usb:2-1 product:MyProduct model:MyModel "
    //     "device:MyDevice transport_id:1"

    if (line[0] == '*') {
        // Garbage lines printed by adb daemon while starting start with a '*'
        return false;
    }

    if (!strncmp("adb server", line, sizeof("adb server") - 1)) {
        // Ignore lines starting with "adb server":
        //   adb server version (41) doesn't match this client (39); killing...
        return false;
    }

    size_t len = strlen(line);

    size_t start;
    size_t end;

    // The serial (the first token) may contain spaces, which are also token
    // separators. To avoid ambiguity, parse the string backwards:
    //  - first, parse all the trailing values until the device state,
    //    identified using a list of well-known values;
    //  - finally, treat the remaining leading token as the device serial.
    //
    // Refs:
    //  - <https://github.com/Genymobile/scrcpy/issues/6248>
    //  - <https://github.com/Genymobile/scrcpy/issues/3537>
    const char *state;
    const char *model = NULL;
    for (;;) {
        locate_last_token(line, len, &start, &end);
        if (start == end) {
            // No more tokens, unexpected
            return false;
        }

        const char *token = &line[start];
        line[end] = '\0';

        if (!strncmp("model:", token, sizeof("model:") - 1)) {
            model = &token[sizeof("model:") - 1];
            // We only need the model
        } else if (is_device_state(token)) {
            state = token;
            // The device state is the item immediately after the device serial
            break;
        }

        // Remove the trailing parts already handled
        len = start;
    }

    assert(state);

    size_t serial_len = rstrip_len(line, start);
    if (!serial_len) {
        // empty serial
        return false;
    }
    char *serial = line;
    line[serial_len] = '\0';

    device->serial = strdup(serial);
    if (!device->serial) {
        return false;
    }

    device->state = strdup(state);
    if (!device->state) {
        free(device->serial);
        return false;
    }

    if (model) {
        device->model = strdup(model);
        if (!device->model) {
            LOG_OOM();
            // model is optional, do not fail
        }
    } else {
        device->model = NULL;
    }

    device->selected = false;

    return true;
}

bool
sc_adb_parse_devices(char *str, struct sc_vec_adb_devices *out_vec) {
#define HEADER "List of devices attached"
#define HEADER_LEN (sizeof(HEADER) - 1)
    bool header_found = false;

    size_t idx_line = 0;
    while (str[idx_line] != '\0') {
        char *line = &str[idx_line];
        size_t len = strcspn(line, "\n");

        // The next line starts after the '\n' (replaced by `\0`)
        idx_line += len;

        if (str[idx_line] != '\0') {
            // The next line starts after the '\n'
            ++idx_line;
        }

        if (!header_found) {
            if (!strncmp(line, HEADER, HEADER_LEN)) {
                header_found = true;
            }
            // Skip everything until the header, there might be garbage lines
            // related to daemon starting before
            continue;
        }

        // The line, but without any trailing '\r'
        size_t line_len = sc_str_remove_trailing_cr(line, len);
        line[line_len] = '\0';

        struct sc_adb_device device;
        bool ok = sc_adb_parse_device(line, &device);
        if (!ok) {
            continue;
        }

        ok = sc_vector_push(out_vec, device);
        if (!ok) {
            LOG_OOM();
            LOGE("Could not push adb_device to vector");
            sc_adb_device_destroy(&device);
            // continue anyway
            continue;
        }
    }

    assert(header_found || out_vec->size == 0);
    return header_found;
}

static char *
sc_adb_parse_device_ip_from_line(char *line) {
    // One line from "ip route" looks like:
    // "192.168.1.0/24 dev wlan0  proto kernel  scope link  src 192.168.1.x"

    // Get the location of the device name (index of "wlan0" in the example)
    ssize_t idx_dev_name = sc_str_index_of_column(line, 2, " ");
    if (idx_dev_name == -1) {
        return NULL;
    }

    // Get the location of the ip address (column 8, but column 6 if we start
    // from column 2). Must be computed before truncating individual columns.
    ssize_t idx_ip = sc_str_index_of_column(&line[idx_dev_name], 6, " ");
    if (idx_ip == -1) {
        return NULL;
    }
    // idx_ip is searched from &line[idx_dev_name]
    idx_ip += idx_dev_name;

    char *dev_name = &line[idx_dev_name];
    size_t dev_name_len = strcspn(dev_name, " \t");
    dev_name[dev_name_len] = '\0';

    char *ip = &line[idx_ip];
    size_t ip_len = strcspn(ip, " \t");
    ip[ip_len] = '\0';

    // Only consider lines where the device name starts with "wlan"
    if (strncmp(dev_name, "wlan", sizeof("wlan") - 1)) {
        LOGD("Device ip lookup: ignoring %s (%s)", ip, dev_name);
        return NULL;
    }

    return strdup(ip);
}

char *
sc_adb_parse_device_ip(char *str) {
    size_t idx_line = 0;
    while (str[idx_line] != '\0') {
        char *line = &str[idx_line];
        size_t len = strcspn(line, "\n");
        bool is_last_line = line[len] == '\0';

        // The same, but without any trailing '\r'
        size_t line_len = sc_str_remove_trailing_cr(line, len);
        line[line_len] = '\0';

        char *ip = sc_adb_parse_device_ip_from_line(line);
        if (ip) {
            // Found
            return ip;
        }

        if (is_last_line) {
            break;
        }

        // The next line starts after the '\n'
        idx_line += len + 1;
    }

    return NULL;
}
