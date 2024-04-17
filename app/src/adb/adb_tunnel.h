#ifndef SC_ADB_TUNNEL_H
#define SC_ADB_TUNNEL_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "options.h"
#include "util/intr.h"
#include "util/net.h"

struct sc_adb_tunnel {
    bool enabled;
    bool forward; // use "adb forward" instead of "adb reverse"
    sc_socket server_socket; // only used if !forward
    uint16_t local_port;
};

/**
 * Initialize the adb tunnel struct to default values
 */
void
sc_adb_tunnel_init(struct sc_adb_tunnel *tunnel);

/**
 * Open a tunnel
 *
 * Blocking calls may be interrupted asynchronously via `intr`.
 *
 * If `force_adb_forward` is not set, then attempts to set up an "adb reverse"
 * tunnel first. Only if it fails (typical on old Android version connected via
 * TCP/IP), use "adb forward".
 */
bool
sc_adb_tunnel_open(struct sc_adb_tunnel *tunnel, struct sc_intr *intr,
                   const char *serial, const char *device_socket_name,
                   struct sc_port_range port_range, bool force_adb_forward);

/**
 * Close the tunnel
 */
bool
sc_adb_tunnel_close(struct sc_adb_tunnel *tunnel, struct sc_intr *intr,
                    const char *serial, const char *device_socket_name);

#endif
