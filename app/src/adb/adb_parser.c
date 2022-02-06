#include "adb_parser.h"

#include <assert.h>
#include <string.h>

#include "util/log.h"
#include "util/str.h"

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
sc_adb_parse_device_ip_from_output(char *str) {
    size_t idx_line = 0;
    while (str[idx_line] != '\0') {
        char *line = &str[idx_line];
        size_t len = strcspn(line, "\n");

        // The same, but without any trailing '\r'
        size_t line_len = sc_str_remove_trailing_cr(line, len);
        line[line_len] = '\0';

        char *ip = sc_adb_parse_device_ip_from_line(line);
        if (ip) {
            // Found
            return ip;
        }

        idx_line += len;

        if (str[idx_line] != '\0') {
            // The next line starts after the '\n'
            idx_line += 1;
        }
    }

    return NULL;
}
