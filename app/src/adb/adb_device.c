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
sc_adb_devices_destroy(struct sc_vec_adb_devices *devices) {
    for (size_t i = 0; i < devices->size; ++i) {
        sc_adb_device_destroy(&devices->data[i]);
    }
    sc_vector_destroy(devices);
}

