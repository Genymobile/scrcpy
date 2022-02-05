#include "adb_device.h"

#include <stdlib.h>

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
sc_adb_devices_destroy_all(struct sc_adb_device *devices, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        sc_adb_device_destroy(&devices[i]);
    }
}

