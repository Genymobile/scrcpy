#include "adb_parser.h"

#include <assert.h>
#include <string.h>

#include "util/log.h"
#include "util/str.h"

static char *
sc_adb_parse_device_ip_from_line(char *line, size_t len) {
    // One line from "ip route" looks lile:
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
    sc_str_truncate(dev_name, len - idx_dev_name + 1, " \t");

    char *ip = &line[idx_ip];
    sc_str_truncate(ip, len - idx_ip + 1, " \t");

    // Only consider lines where the device name starts with "wlan"
    if (strncmp(dev_name, "wlan", sizeof("wlan") - 1)) {
        LOGD("Device ip lookup: ignoring %s (%s)", ip, dev_name);
        return NULL;
    }

    return strdup(ip);
}

char *
sc_adb_parse_device_ip_from_output(char *buf, size_t buf_len) {
    size_t idx_line = 0;
    while (idx_line < buf_len && buf[idx_line] != '\0') {
        char *line = &buf[idx_line];
        size_t len = sc_str_truncate(line, buf_len - idx_line, "\n");

        // The same, but without any trailing '\r'
        size_t line_len = sc_str_remove_trailing_cr(line, len);

        char *ip = sc_adb_parse_device_ip_from_line(line, line_len);
        if (ip) {
            // Found
            return ip;
        }

        // The next line starts after the '\n' (replaced by `\0`)
        idx_line += len + 1;
    }

    return NULL;
}
