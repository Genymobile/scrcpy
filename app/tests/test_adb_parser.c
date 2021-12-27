#include "common.h"

#include <assert.h>

#include "adb_parser.h"

static void test_get_ip_single_line() {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.12.34\r\r\n";

    char *ip = sc_adb_parse_device_ip_from_output(ip_route, sizeof(ip_route));
    assert(ip);
    assert(!strcmp(ip, "192.168.12.34"));
}

static void test_get_ip_single_line_without_eol() {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.12.34";

    char *ip = sc_adb_parse_device_ip_from_output(ip_route, sizeof(ip_route));
    assert(ip);
    assert(!strcmp(ip, "192.168.12.34"));
}

static void test_get_ip_single_line_with_trailing_space() {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.12.34 \n";

    char *ip = sc_adb_parse_device_ip_from_output(ip_route, sizeof(ip_route));
    assert(ip);
    assert(!strcmp(ip, "192.168.12.34"));
}

static void test_get_ip_multiline_first_ok() {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.1.2\r\n"
                      "10.0.0.0/24 dev rmnet  proto kernel  scope link  src "
                      "10.0.0.2\r\n";

    char *ip = sc_adb_parse_device_ip_from_output(ip_route, sizeof(ip_route));
    assert(ip);
    assert(!strcmp(ip, "192.168.1.2"));
}

static void test_get_ip_multiline_second_ok() {
    char ip_route[] = "10.0.0.0/24 dev rmnet  proto kernel  scope link  src "
                      "10.0.0.3\r\n"
                      "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.1.3\r\n";

    char *ip = sc_adb_parse_device_ip_from_output(ip_route, sizeof(ip_route));
    assert(ip);
    assert(!strcmp(ip, "192.168.1.3"));
}

static void test_get_ip_no_wlan() {
    char ip_route[] = "192.168.1.0/24 dev rmnet  proto kernel  scope link  src "
                      "192.168.12.34\r\r\n";

    char *ip = sc_adb_parse_device_ip_from_output(ip_route, sizeof(ip_route));
    assert(!ip);
}

static void test_get_ip_truncated() {
    char ip_route[] = "192.168.1.0/24 dev rmnet  proto kernel  scope link  src "
                      "\n";

    char *ip = sc_adb_parse_device_ip_from_output(ip_route, sizeof(ip_route));
    assert(!ip);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_get_ip_single_line();
    test_get_ip_single_line_without_eol();
    test_get_ip_single_line_with_trailing_space();
    test_get_ip_multiline_first_ok();
    test_get_ip_multiline_second_ok();
    test_get_ip_no_wlan();
    test_get_ip_truncated();
}
