#ifndef SC_ADB_PARSER_H
#define SC_ADB_PARSER_H

#include "common.h"

#include <stdbool.h>

#include "adb/adb_device.h"

/**
 * Parse the available devices from the output of `adb devices`
 *
 * The parameter must be a NUL-terminated string.
 *
 * Warning: this function modifies the buffer for optimization purposes.
 */
bool
sc_adb_parse_devices(char *str, struct sc_vec_adb_devices *out_vec);

/**
 * Parse the ip from the output of `adb shell ip route`
 *
 * The parameter must be a NUL-terminated string.
 *
 * Warning: this function modifies the buffer for optimization purposes.
 */
char *
sc_adb_parse_device_ip(char *str);

#endif
