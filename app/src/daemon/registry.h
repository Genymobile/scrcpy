#ifndef SC_DAEMON_REGISTRY_H
#define SC_DAEMON_REGISTRY_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * On-disk daemon registry (doc/daemon.md §7): one JSON file per daemon,
 * <dir>/<port>.json, written atomically. Advisory only — clients passing
 * --client-port never need it, so all failures here are warnings.
 */

#define SC_REGISTRY_VERSION 1

/**
 * Write (create or update) the registry entry for this daemon.
 *
 * `serial`, `device_name` and `state` may be NULL/empty during startup.
 */
bool
sc_registry_write(uint16_t port, const char *serial, const char *device_name,
                  const char *state);

/**
 * Remove the registry entry (on graceful shutdown).
 */
void
sc_registry_remove(uint16_t port);

#endif
