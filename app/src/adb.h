#ifndef SC_ADB_H
#define SC_ADB_H

#include "common.h"

#include <stdbool.h>
#include <inttypes.h>

#include "util/intr.h"

#define SC_ADB_NO_STDOUT (1 << 0)
#define SC_ADB_NO_STDERR (1 << 1)
#define SC_ADB_NO_LOGERR (1 << 2)

sc_pid
adb_execute(const char *serial, const char *const adb_cmd[], size_t len,
            unsigned flags);

bool
adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
            const char *device_socket_name, unsigned flags);

bool
adb_forward_remove(struct sc_intr *intr, const char *serial,
                   uint16_t local_port, unsigned flags);

bool
adb_reverse(struct sc_intr *intr, const char *serial,
            const char *device_socket_name, uint16_t local_port,
            unsigned flags);

bool
adb_reverse_remove(struct sc_intr *intr, const char *serial,
                   const char *device_socket_name, unsigned flags);

bool
adb_push(struct sc_intr *intr, const char *serial, const char *local,
         const char *remote, unsigned flags);

bool
adb_install(struct sc_intr *intr, const char *serial, const char *local,
            unsigned flags);

/**
 * Execute `adb tcpip <port>`
 */
bool
adb_tcpip(struct sc_intr *intr, const char *serial, uint16_t port,
          unsigned flags);

/**
 * Execute `adb connect <ip_port>`
 *
 * `ip_port` may not be NULL.
 */
bool
adb_connect(struct sc_intr *intr, const char *ip_port, unsigned flags);

/**
 * Execute `adb disconnect [<ip_port>]`
 *
 * If `ip_port` is NULL, execute `adb disconnect`.
 * Otherwise, execute `adb disconnect <ip_port>`.
 */
bool
adb_disconnect(struct sc_intr *intr, const char *ip_port, unsigned flags);

/**
 * Execute `adb get-serialno`
 *
 * Return the result, to be freed by the caller, or NULL on error.
 */
char *
adb_get_serialno(struct sc_intr *intr, unsigned flags);

/**
 * Attempt to retrieve the device IP
 *
 * Return the IP as a string of the form "xxx.xxx.xxx.xxx", to be freed by the
 * caller, or NULL on error.
 */
char *
adb_get_device_ip(struct sc_intr *intr, const char *serial, unsigned flags);

#endif
