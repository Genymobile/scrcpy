#ifndef SC_CONFIG_FILE_H
#define SC_CONFIG_FILE_H

#include "common.h"

#include <stdbool.h>

struct sc_config_argv {
    char **argv;   // merged argv array
    int argc;      // merged argc
    char **allocs; // heap strings to free later
    int nallocs;
};

// Build a merged argv from config file + real argv.
// CLI args come after config args so they override ("last wins").
// Returns false on allocation failure.
bool
sc_config_argv_init(struct sc_config_argv *ca, int argc, char *argv[]);

void
sc_config_argv_destroy(struct sc_config_argv *ca);

#endif
