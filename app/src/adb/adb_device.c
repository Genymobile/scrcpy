#include "adb_device.h"

#include <stdlib.h>
#include <string.h>

void
sc_adb_device_destroy(struct sc_adb_device *device) {
    free(device->serial);
    free(device->state);
    free(device->model);
}

void
sc_adb_device_move(struct sc_adb_device *dst, struct sc_adb_device *src) {
    *dst = *src;
    src->serial = NULL;
    src->state = NULL;
    src->model = NULL;
}

void
sc_adb_devices_destroy(struct sc_vec_adb_devices *devices) {
    for (size_t i = 0; i < devices->size; ++i) {
        sc_adb_device_destroy(&devices->data[i]);
    }
    sc_vector_destroy(devices);
}

enum sc_adb_device_type
sc_adb_device_get_type(const char *serial) {
    // Starts with "emulator-"
    if (!strncmp(serial, "emulator-", sizeof("emulator-") - 1)) {
        return SC_ADB_DEVICE_TYPE_EMULATOR;
    }

    // If the serial contains a ':', then it is a TCP/IP device (it is
    // sufficient to distinguish an ip:port from a real USB serial)
    if (strchr(serial, ':')) {
        return SC_ADB_DEVICE_TYPE_TCPIP;
    }

    // TCP/IP devices provided by mDNS contain "adb-tls-connect"
    // <https://github.com/Genymobile/scrcpy/issues/6248>
    if (strstr(serial, "adb-tls-connect")) {
        return SC_ADB_DEVICE_TYPE_TCPIP;
    }

    return SC_ADB_DEVICE_TYPE_USB;
}
