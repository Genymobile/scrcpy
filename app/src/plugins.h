#ifndef SC_PLUGINS_H
#define SC_PLUGINS_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * Plugin management (doc/plugins.md).
 *
 * Installed plugins live under ~/.scrcpy-auto/plugins/<name>/, each a
 * self-contained add-on directory whose entrypoint is entrypoint.sh. Plugins
 * are installed by copying the matching directory out of a git repository (a
 * single repository may hold several plugins in different sub-directories):
 *
 *   scrcpy-auto plugins-install <git-url> --add-on-name=<name>|ALL
 *   scrcpy-auto plugins-upgrade <git-url> --add-on-name=<name>|ALL
 *
 * ALL installs every plugin directory in the repository (or, for upgrade, every
 * one that is already installed). Once installed, --add-on accepts the bare
 * plugin name, resolved to its entrypoint (see sc_plugins_resolve_addon()).
 *
 * Plugin management is a Unix/macOS feature, like add-ons themselves; on Windows
 * both entry points are no-ops.
 */

/**
 * Handle a `plugins-install` / `plugins-upgrade` subcommand.
 *
 * argv[1] is the subcommand, argv[2..] the git repository URL and
 * --add-on-name=<name>|ALL (in any order). Returns a process exit code
 * (0 on success).
 */
int
sc_plugins_cli(int argc, char *argv[]);

/**
 * Resolve a bare `--add-on` plugin name to its entrypoint path.
 *
 * If `spec` looks like a filesystem path (it contains '/' or names an existing
 * file), it is left untouched and false is returned (the caller uses `spec`
 * as-is). Otherwise `spec` is treated as an installed plugin name: `buf` is
 * filled with "<HOME>/.scrcpy-auto/plugins/<spec>/entrypoint.sh" and true is
 * returned (false on truncation or if HOME is unset).
 */
bool
sc_plugins_resolve_addon(const char *spec, char *buf, size_t size);

#endif
