#ifndef SC_USER_CONFIG_H
#define SC_USER_CONFIG_H

#include "common.h"

#include <stdbool.h>

/**
 * Ensure that the current user's scrcpy-auto directory exists.
 *
 * On Unix, create "$HOME/.scrcpy-auto" with private permissions if it does not
 * already exist. Existing directories and their permissions are left
 * untouched. Returns false if HOME is unavailable, the path exists but is not
 * a directory, or the directory cannot be created.
 *
 * On Windows, this is a no-op which returns true.
 */
bool
sc_user_config_ensure_dir(void);

#endif
