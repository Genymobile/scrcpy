#ifndef SC_DAEMON_ADDON_H
#define SC_DAEMON_ADDON_H

#include "common.h"

#include <stdbool.h>

#include "options.h" // SC_MAX_ADDONS
#include "util/process.h"

/**
 * Plugin add-ons (doc/addons.md).
 *
 * An add-on is an entrypoint script that provides a custom --<command> option.
 * At daemon startup each script is queried once (SC_ADDON_MODE=register) for
 * its command name; when a client sends that command, the daemon runs the
 * script (SC_ADDON_MODE=run) with the command value as $1 and runtime info in
 * environment variables.
 */
struct sc_addon {
    char *name; // command name reported by the script
    char *path; // entrypoint script path
};

struct sc_addons {
    struct sc_addon list[SC_MAX_ADDONS];
    unsigned count;
};

/**
 * Load add-ons: query each script for its command name and register it.
 * Scripts that fail to register (no name, error) are skipped with a warning.
 */
void
sc_addons_load(struct sc_addons *addons, const char *const *paths,
               unsigned count);

/**
 * Return the entrypoint path registered for `name`, or NULL.
 */
const char *
sc_addons_find(const struct sc_addons *addons, const char *name);

void
sc_addons_destroy(struct sc_addons *addons);

/**
 * Run an add-on entrypoint: `env <env...> SC_ADDON_MODE=run <path> <args>`.
 * stdio is inherited so the script's output appears in the daemon log. On
 * success writes the child pid to `*pid` (the caller waits/terminates it).
 */
bool
sc_addon_run(const char *path, const char *args, const char *const env[],
             unsigned env_count, sc_pid *pid);

#endif
