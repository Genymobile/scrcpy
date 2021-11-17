#ifndef SC_ADB_H
#define SC_ADB_H

#include "common.h"

#include <stdbool.h>
#include <inttypes.h>

#include "util/process.h"

sc_pid
adb_execute(const char *serial, const char *const adb_cmd[], size_t len);

sc_pid
adb_execute_p(const char *serial, const char *const adb_cmd[], size_t len,
              sc_pipe *pout);

sc_pid
adb_forward(const char *serial, uint16_t local_port,
            const char *device_socket_name);

sc_pid
adb_forward_remove(const char *serial, uint16_t local_port);

sc_pid
adb_reverse(const char *serial, const char *device_socket_name,
            uint16_t local_port);

sc_pid
adb_reverse_remove(const char *serial, const char *device_socket_name);

sc_pid
adb_push(const char *serial, const char *local, const char *remote);

sc_pid
adb_install(const char *serial, const char *local);

/**
 * Execute `adb get-serialno`
 *
 * The result can be read from the output parameter `pout`.
 */
sc_pid
adb_get_serialno(sc_pipe *pout);

#endif
