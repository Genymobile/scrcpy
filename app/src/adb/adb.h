#ifndef SC_ADB_H
#define SC_ADB_H

#include "common.h"

#include <stdbool.h>
#include <inttypes.h>

#include "util/intr.h"

#define SC_ADB_NO_STDOUT (1 << 0)
#define SC_ADB_NO_STDERR (1 << 1)
#define SC_ADB_NO_LOGERR (1 << 2)

#define SC_ADB_SILENT (SC_ADB_NO_STDOUT | SC_ADB_NO_STDERR | SC_ADB_NO_LOGERR)

const char *
sc_adb_get_executable(void);

sc_pid
sc_adb_execute(const char *const argv[], unsigned flags);

bool
sc_adb_forward(struct sc_intr *intr, const char *serial, uint16_t local_port,
               const char *device_socket_name, unsigned flags);

bool
sc_adb_forward_remove(struct sc_intr *intr, const char *serial,
                      uint16_t local_port, unsigned flags);

bool
sc_adb_reverse(struct sc_intr *intr, const char *serial,
               const char *device_socket_name, uint16_t local_port,
               unsigned flags);

bool
sc_adb_reverse_remove(struct sc_intr *intr, const char *serial,
                      const char *device_socket_name, unsigned flags);

bool
sc_adb_push(struct sc_intr *intr, const char *serial, const char *local,
            const char *remote, unsigned flags);

bool
sc_adb_install(struct sc_intr *intr, const char *serial, const char *local,
               unsigned flags);

/**
 * Execute `adb tcpip <port>`
 */
bool
sc_adb_tcpip(struct sc_intr *intr, const char *serial, uint16_t port,
             unsigned flags);

/**
 * Execute `adb connect <ip_port>`
 *
 * `ip_port` may not be NULL.
 */
bool
sc_adb_connect(struct sc_intr *intr, const char *ip_port, unsigned flags);

/**
 * Execute `adb disconnect [<ip_port>]`
 *
 * If `ip_port` is NULL, execute `adb disconnect`.
 * Otherwise, execute `adb disconnect <ip_port>`.
 */
bool
sc_adb_disconnect(struct sc_intr *intr, const char *ip_port, unsigned flags);

/**
 * Execute `adb getprop <prop>`
 */
char *
sc_adb_getprop(struct sc_intr *intr, const char *serial, const char *prop,
               unsigned flags);

/**
 * Execute `adb get-serialno`
 *
 * Return the result, to be freed by the caller, or NULL on error.
 */
char *
sc_adb_get_serialno(struct sc_intr *intr, unsigned flags);

/**
 * Attempt to retrieve the device IP
 *
 * Return the IP as a string of the form "xxx.xxx.xxx.xxx", to be freed by the
 * caller, or NULL on error.
 */
char *
sc_adb_get_device_ip(struct sc_intr *intr, const char *serial, unsigned flags);

/**
 * Indicate if the serial represents an IP address
 *
 * In practice, it just returns true if and only if it contains a ':', which is
 * sufficient to distinguish an ip:port from a real USB serial.
 */
bool
sc_adb_is_serial_tcpip(const char *serial);

#endif
