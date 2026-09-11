#ifndef SC_CONFIG_FILE_H
#define SC_CONFIG_FILE_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>

struct sc_config_argv {
    char **argv;
    int argc;
    char **owned_args;
    size_t owned_argc;
};

/**
 * Prepend global and selected profile options from the configuration file to
 * the command-line arguments.
 *
 * The result must be destroyed by sc_config_argv_destroy().
 */
bool
sc_config_argv_init(struct sc_config_argv *ca, int argc, char *argv[],
                    const char *config_path, bool config_disabled,
                    const char *profile);

void
sc_config_argv_destroy(struct sc_config_argv *ca);

#endif
