#ifndef SC_ADB_DEVICE_H
#define SC_ADB_DEVICE_H

#include "common.h"

#include <stdbool.h>

#include "util/vector.h"

struct sc_adb_device {
    char *serial;
    char *state;
    char *model;
    bool selected;
};

enum sc_adb_device_type {
    SC_ADB_DEVICE_TYPE_USB,
    SC_ADB_DEVICE_TYPE_TCPIP,
    SC_ADB_DEVICE_TYPE_EMULATOR,
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

/**
 * Deduce the device type from the serial
 */
enum sc_adb_device_type
sc_adb_device_get_type(const char *serial);

#endif
