#ifndef SC_ADB_DEVICE_H
#define SC_ADB_DEVICE_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>

struct sc_adb_device {
    char *serial;
    char *state;
    char *model;
};

void
sc_adb_device_destroy(struct sc_adb_device *device);

/**
 * Move src to dest
 *
 * After this call, the content of src is undefined, except that
 * sc_adb_device_destroy() can be called.
 *
 * This is useful to take a device from a list that will be destroyed, without
 * making unnecessary copies.
 */
void
sc_adb_device_move(struct sc_adb_device *dst, struct sc_adb_device *src);

void
sc_adb_devices_destroy_all(struct sc_adb_device *devices, size_t count);

#endif
