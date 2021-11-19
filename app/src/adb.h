#ifndef SC_ADB_H
#define SC_ADB_H

#include "common.h"

#include <stdbool.h>
#include <inttypes.h>

#include "util/intr.h"

sc_pid
adb_execute(const char *serial, const char *const adb_cmd[], size_t len,
            unsigned inherit);

bool
adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
            const char *device_socket_name, unsigned inherit);

bool
adb_forward_remove(struct sc_intr *intr, const char *serial,
                   uint16_t local_port, unsigned inherit);

bool
adb_reverse(struct sc_intr *intr, const char *serial,
            const char *device_socket_name, uint16_t local_port,
            unsigned inherit);

bool
adb_reverse_remove(struct sc_intr *intr, const char *serial,
                   const char *device_socket_name, unsigned inherit);

bool
adb_push(struct sc_intr *intr, const char *serial, const char *local,
         const char *remote, unsigned inherit);

bool
adb_install(struct sc_intr *intr, const char *serial, const char *local,
            unsigned inherit);

/**
 * Execute `adb get-serialno`
 *
 * Return the result, to be freed by the caller, or NULL on error.
 */
char *
adb_get_serialno(struct sc_intr *intr, unsigned inherit);

#endif
