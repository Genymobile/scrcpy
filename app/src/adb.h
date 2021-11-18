#ifndef SC_ADB_H
#define SC_ADB_H

#include "common.h"

#include <stdbool.h>
#include <inttypes.h>

#include "util/process.h"
#include "util/intr.h"

sc_pid
adb_execute(const char *serial, const char *const adb_cmd[], size_t len);

sc_pid
adb_execute_p(const char *serial, const char *const adb_cmd[], size_t len,
              sc_pipe *pout);

sc_pid
adb_exec_forward(const char *serial, uint16_t local_port,
                 const char *device_socket_name);

sc_pid
adb_exec_forward_remove(const char *serial, uint16_t local_port);

sc_pid
adb_exec_reverse(const char *serial, const char *device_socket_name,
                 uint16_t local_port);

sc_pid
adb_exec_reverse_remove(const char *serial, const char *device_socket_name);

sc_pid
adb_exec_push(const char *serial, const char *local, const char *remote);

sc_pid
adb_exec_install(const char *serial, const char *local);

/**
 * Execute `adb get-serialno`
 *
 * The result can be read from the output parameter `pout`.
 */
sc_pid
adb_exec_get_serialno(sc_pipe *pout);

bool
adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
            const char *device_socket_name);

bool
adb_forward_remove(struct sc_intr *intr, const char *serial,
                   uint16_t local_port);

bool
adb_reverse(struct sc_intr *intr, const char *serial,
            const char *device_socket_name, uint16_t local_port);

bool
adb_reverse_remove(struct sc_intr *intr, const char *serial,
                   const char *device_socket_name);

bool
adb_push(struct sc_intr *intr, const char *serial, const char *local,
         const char *remote);

bool
adb_install(struct sc_intr *intr, const char *serial, const char *local);

/**
 * Execute `adb get-serialno`
 *
 * Return the result, to be freed by the caller, or NULL on error.
 */
char *
adb_get_serialno(struct sc_intr *intr);

#endif
