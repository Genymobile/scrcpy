#ifndef SC_ADB_PARSER_H
#define SC_ADB_PARSER_H

#include "common.h"

#include <stddef.h>

/**
 * Parse the ip from the output of `adb shell ip route`
 *
 * The parameter must be a NUL-terminated string.
 *
 * Warning: this function modifies the buffer for optimization purposes.
 */
char *
sc_adb_parse_device_ip_from_output(char *str);

#endif
