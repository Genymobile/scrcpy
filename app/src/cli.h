#ifndef SCRCPY_CLI_H
#define SCRCPY_CLI_H

#include "common.h"

#include <stdbool.h>

#include "options.h"

enum sc_pause_on_exit {
    SC_PAUSE_ON_EXIT_TRUE,
    SC_PAUSE_ON_EXIT_FALSE,
    SC_PAUSE_ON_EXIT_IF_ERROR,
};

struct scrcpy_cli_args {
    struct scrcpy_options opts;
    bool help;
    bool version;
    enum sc_pause_on_exit pause_on_exit;
};

void
scrcpy_print_usage(const char *arg0);

bool
scrcpy_parse_args(struct scrcpy_cli_args *args, int argc, char *argv[]);

#ifdef SC_TEST
bool
sc_parse_shortcut_mods(const char *s, struct sc_shortcut_mods *mods);
#endif

#endif
