#ifndef SC_ADB_H
#define SC_ADB_H

#include "common.h"

#include <stdbool.h>
#include <inttypes.h>

#include "adb_device.h"
#include "util/intr.h"

#define SC_ADB_NO_STDOUT (1 << 0)
#define SC_ADB_NO_STDERR (1 << 1)
#define SC_ADB_NO_LOGERR (1 << 2)

#define SC_ADB_SILENT (SC_ADB_NO_STDOUT | SC_ADB_NO_STDERR | SC_ADB_NO_LOGERR)

bool
sc_adb_init(void);

void
sc_adb_destroy(void);

const char *
sc_adb_get_executable(void);

enum sc_adb_device_selector_type {
    SC_ADB_DEVICE_SELECT_ALL,
    SC_ADB_DEVICE_SELECT_SERIAL,
    SC_ADB_DEVICE_SELECT_USB,
    SC_ADB_DEVICE_SELECT_TCPIP,
};

struct sc_adb_device_selector {
    enum sc_adb_device_selector_type type;
    const char *serial;
};

sc_pid
sc_adb_execute(const char *const argv[], unsigned flags);

bool
sc_adb_start_server(struct sc_intr *intr, unsigned flags);

bool
sc_adb_kill_server(struct sc_intr *intr, unsigned flags);

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
 * Execute `adb devices` and parse the result to select a device
 *
 * Return true if a single matching device is found, and write it to out_device.
 */
bool
sc_adb_select_device(struct sc_intr *intr,
                     const struct sc_adb_device_selector *selector,
                     unsigned flags, struct sc_adb_device *out_device);

/**
 * Execute `adb getprop <prop>`
 */
char *
sc_adb_getprop(struct sc_intr *intr, const char *serial, const char *prop,
               unsigned flags);

/**
 * Attempt to retrieve the device IP
 *
 * Return the IP as a string of the form "xxx.xxx.xxx.xxx", to be freed by the
 * caller, or NULL on error.
 */
char *
sc_adb_get_device_ip(struct sc_intr *intr, const char *serial, unsigned flags);

/**
 * Return the device SDK version.
 */
uint16_t
sc_adb_get_device_sdk_version(struct sc_intr *intr, const char *serial);

#endif
