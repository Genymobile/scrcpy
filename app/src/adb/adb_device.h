#ifndef SC_ADB_DEVICE_H
#define SC_ADB_DEVICE_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>

#include "util/vector.h"

struct sc_adb_device {
    char *serial;
    char *state;
    char *model;
    bool selected;
};

struct sc_vec_adb_devices SC_VECTOR(struct sc_adb_device);

void
sc_adb_device_destroy(struct sc_adb_device *device);

/**
 * Move src to dst
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
sc_adb_devices_destroy(struct sc_vec_adb_devices *devices);

#endif
