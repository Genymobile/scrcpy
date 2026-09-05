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
 * its command name and optional extra argument declarations; when a client
 * sends that command, the daemon runs the script (SC_ADDON_MODE=run) with the
 * command value as $1 and every declared argument exported as SC_ARG_<NAME>
 * (name upper-cased, '-' -> '_'), alongside the usual runtime env vars.
 *
 * The register output is line based (Unified Plugin Protocol, doc/addons.md):
 *   name=<command>                    (or a bare command name, for back-compat)
 *   result=<field>                    (optional: result field name)
 *   service=true                      (optional: long-running service, below)
 *   arg=<name>:<string|list|path|pathlist>:<required|optional>   (extra args)
 *   env=<NAME>:<required|optional>[:<description>]   (declared env var)
 *   meta=<key>:<value>                (free-form metadata)
 *
 * A `service=true` add-on may keep running after it writes its result: the
 * daemon waits until the result file is ready (or the process exits), returns
 * that result to the client, and if the process is still alive it is ADOPTED
 * — tracked by the daemon and terminated when the daemon shuts down. The same
 * add-on can also do one-shot work (write result, exit) in the same run mode;
 * whichever it does, the daemon reacts to whether the process stayed alive.
 * The command's own value is an implicit required string arg; only additional
 * arguments need an `arg=` line. A `path`/`pathlist` arg holds file paths the
 * daemon must be able to read (the client sends them as-is when local, or
 * uploads the bytes and substitutes daemon-side paths when remote).
 */
struct sc_addon_arg {
    char *name;     // argument name, e.g. "ref-images"
    bool is_list;   // may be given more than once / holds multiple values
    bool is_path;   // file path(s): resolved locally or uploaded when remote
    bool required;  // client must supply it when the command is invoked
};

struct sc_addon_env {
    char *name;        // environment variable name, e.g. "ARK_API_KEY"
    bool required;     // fail the run if unset in the daemon environment
    char *description; // may be NULL
};

struct sc_addon_meta {
    char *key;
    char *value;
};

struct sc_addon {
    char *name; // command name reported by the script
    char *path; // entrypoint script path
    char *result_field; // result field name (result=), NULL if not declared
    bool service; // long-running: launched detached, tracked, killed on exit
    struct sc_addon_arg args[SC_MAX_ADDON_ARGS]; // extra declared arguments
    unsigned arg_count;
    struct sc_addon_env envs[SC_MAX_ADDON_ENVS]; // declared env vars
    unsigned env_count;
    struct sc_addon_meta metas[SC_MAX_ADDON_META]; // free-form metadata
    unsigned meta_count;
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

/**
 * Return the registered add-on for `name` (schema included), or NULL.
 */
const struct sc_addon *
sc_addons_get(const struct sc_addons *addons, const char *name);

/**
 * Return the name of the first declared `required` env var that is unset in the
 * daemon's environment, or NULL if all required vars are present.
 */
const char *
sc_addon_missing_env(const struct sc_addon *a);

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
