#ifndef SC_ADB_H
#define SC_ADB_H

#include "common.h"

#include <stdbool.h>
#include <inttypes.h>

#include "util/process.h"

process_t
adb_execute(const char *serial, const char *const adb_cmd[], size_t len);

process_t
adb_forward(const char *serial, uint16_t local_port,
            const char *device_socket_name);

process_t
adb_forward_remove(const char *serial, uint16_t local_port);

process_t
adb_reverse(const char *serial, const char *device_socket_name,
            uint16_t local_port);

process_t
adb_reverse_remove(const char *serial, const char *device_socket_name);

process_t
adb_push(const char *serial, const char *local, const char *remote);

process_t
adb_install(const char *serial, const char *local);

#endif
