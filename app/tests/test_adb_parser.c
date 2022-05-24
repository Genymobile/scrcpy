#include "common.h"

#include <assert.h>

#include "adb/adb_device.h"
#include "adb/adb_parser.h"

static void test_adb_devices(void) {
    char output[] =
        "List of devices attached\n"
        "0123456789abcdef	device usb:2-1 product:MyProduct model:MyModel "
            "device:MyDevice transport_id:1\n"
        "192.168.1.1:5555	device product:MyWifiProduct model:MyWifiModel "
            "device:MyWifiDevice trandport_id:2\n";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(ok);
    assert(vec.size == 2);

    struct sc_adb_device *device = &vec.data[0];
    assert(!strcmp("0123456789abcdef", device->serial));
    assert(!strcmp("device", device->state));
    assert(!strcmp("MyModel", device->model));

    device = &vec.data[1];
    assert(!strcmp("192.168.1.1:5555", device->serial));
    assert(!strcmp("device", device->state));
    assert(!strcmp("MyWifiModel", device->model));

    sc_adb_devices_destroy(&vec);
}

static void test_adb_devices_cr(void) {
    char output[] =
        "List of devices attached\r\n"
        "0123456789abcdef	device usb:2-1 product:MyProduct model:MyModel "
            "device:MyDevice transport_id:1\r\n"
        "192.168.1.1:5555	device product:MyWifiProduct model:MyWifiModel "
            "device:MyWifiDevice trandport_id:2\r\n";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(ok);
    assert(vec.size == 2);

    struct sc_adb_device *device = &vec.data[0];
    assert(!strcmp("0123456789abcdef", device->serial));
    assert(!strcmp("device", device->state));
    assert(!strcmp("MyModel", device->model));

    device = &vec.data[1];
    assert(!strcmp("192.168.1.1:5555", device->serial));
    assert(!strcmp("device", device->state));
    assert(!strcmp("MyWifiModel", device->model));

    sc_adb_devices_destroy(&vec);
}

static void test_adb_devices_daemon_start(void) {
    char output[] =
        "* daemon not running; starting now at tcp:5037\n"
        "* daemon started successfully\n"
        "List of devices attached\n"
        "0123456789abcdef	device usb:2-1 product:MyProduct model:MyModel "
            "device:MyDevice transport_id:1\n";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(ok);
    assert(vec.size == 1);

    struct sc_adb_device *device = &vec.data[0];
    assert(!strcmp("0123456789abcdef", device->serial));
    assert(!strcmp("device", device->state));
    assert(!strcmp("MyModel", device->model));

    sc_adb_devices_destroy(&vec);
}

static void test_adb_devices_daemon_start_mixed(void) {
    char output[] =
        "List of devices attached\n"
        "adb server version (41) doesn't match this client (39); killing...\n"
        "* daemon started successfully *\n"
        "0123456789abcdef	unauthorized usb:1-1\n"
        "87654321	device usb:2-1 product:MyProduct model:MyModel "
            "device:MyDevice\n";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(ok);
    assert(vec.size == 2);

    struct sc_adb_device *device = &vec.data[0];
    assert(!strcmp("0123456789abcdef", device->serial));
    assert(!strcmp("unauthorized", device->state));
    assert(!device->model);

    device = &vec.data[1];
    assert(!strcmp("87654321", device->serial));
    assert(!strcmp("device", device->state));
    assert(!strcmp("MyModel", device->model));

    sc_adb_devices_destroy(&vec);
}

static void test_adb_devices_without_eol(void) {
    char output[] =
        "List of devices attached\n"
        "0123456789abcdef	device usb:2-1 product:MyProduct model:MyModel "
            "device:MyDevice transport_id:1";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(ok);
    assert(vec.size == 1);

    struct sc_adb_device *device = &vec.data[0];
    assert(!strcmp("0123456789abcdef", device->serial));
    assert(!strcmp("device", device->state));
    assert(!strcmp("MyModel", device->model));

    sc_adb_devices_destroy(&vec);
}

static void test_adb_devices_without_header(void) {
    char output[] =
        "0123456789abcdef	device usb:2-1 product:MyProduct model:MyModel "
            "device:MyDevice transport_id:1\n";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(!ok);
}

static void test_adb_devices_corrupted(void) {
    char output[] =
        "List of devices attached\n"
        "corrupted_garbage\n";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(ok);
    assert(vec.size == 0);
}

static void test_adb_devices_spaces(void) {
    char output[] =
        "List of devices attached\n"
        "0123456789abcdef       unauthorized usb:1-4 transport_id:3\n";

    struct sc_vec_adb_devices vec = SC_VECTOR_INITIALIZER;
    bool ok = sc_adb_parse_devices(output, &vec);
    assert(ok);
    assert(vec.size == 1);

    struct sc_adb_device *device = &vec.data[0];
    assert(!strcmp("0123456789abcdef", device->serial));
    assert(!strcmp("unauthorized", device->state));
    assert(!device->model);

    sc_adb_devices_destroy(&vec);
}

static void test_get_ip_single_line(void) {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.12.34\r\r\n";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(ip);
    assert(!strcmp(ip, "192.168.12.34"));
    free(ip);
}

static void test_get_ip_single_line_without_eol(void) {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.12.34";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(ip);
    assert(!strcmp(ip, "192.168.12.34"));
    free(ip);
}

static void test_get_ip_single_line_with_trailing_space(void) {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.12.34 \n";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(ip);
    assert(!strcmp(ip, "192.168.12.34"));
    free(ip);
}

static void test_get_ip_multiline_first_ok(void) {
    char ip_route[] = "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.1.2\r\n"
                      "10.0.0.0/24 dev rmnet  proto kernel  scope link  src "
                      "10.0.0.2\r\n";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(ip);
    assert(!strcmp(ip, "192.168.1.2"));
    free(ip);
}

static void test_get_ip_multiline_second_ok(void) {
    char ip_route[] = "10.0.0.0/24 dev rmnet  proto kernel  scope link  src "
                      "10.0.0.3\r\n"
                      "192.168.1.0/24 dev wlan0  proto kernel  scope link  src "
                      "192.168.1.3\r\n";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(ip);
    assert(!strcmp(ip, "192.168.1.3"));
    free(ip);
}

static void test_get_ip_no_wlan(void) {
    char ip_route[] = "192.168.1.0/24 dev rmnet  proto kernel  scope link  src "
                      "192.168.12.34\r\r\n";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(!ip);
}

static void test_get_ip_no_wlan_without_eol(void) {
    char ip_route[] = "192.168.1.0/24 dev rmnet  proto kernel  scope link  src "
                      "192.168.12.34";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(!ip);
}

static void test_get_ip_truncated(void) {
    char ip_route[] = "192.168.1.0/24 dev rmnet  proto kernel  scope link  src "
                      "\n";

    char *ip = sc_adb_parse_device_ip(ip_route);
    assert(!ip);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_adb_devices();
    test_adb_devices_cr();
    test_adb_devices_daemon_start();
    test_adb_devices_daemon_start_mixed();
    test_adb_devices_without_eol();
    test_adb_devices_without_header();
    test_adb_devices_corrupted();
    test_adb_devices_spaces();

    test_get_ip_single_line();
    test_get_ip_single_line_without_eol();
    test_get_ip_single_line_with_trailing_space();
    test_get_ip_multiline_first_ok();
    test_get_ip_multiline_second_ok();
    test_get_ip_no_wlan();
    test_get_ip_no_wlan_without_eol();
    test_get_ip_truncated();

    return 0;
}
