#ifndef SCRCPY_CLI_H
#define SCRCPY_CLI_H

#include "common.h"

#include <stdbool.h>

#include "options.h"

enum sc_pause_on_exit {
    SC_PAUSE_ON_EXIT_UNDEFINED,
    SC_PAUSE_ON_EXIT_TRUE,
    SC_PAUSE_ON_EXIT_FALSE,
    SC_PAUSE_ON_EXIT_IF_ERROR,
};

struct scrcpy_cli_args {
    struct scrcpy_options opts;
    const char *profile;
    bool help;
    bool version;
    enum sc_pause_on_exit pause_on_exit;
};

struct scrcpy_cli_preparse {
    const char *config_path;
    const char *profile;
    bool config_disabled;
};

void
scrcpy_print_usage(const char *arg0);

bool
scrcpy_preparse_args(int argc, char *argv[],
                     struct scrcpy_cli_preparse *preparse);

bool
scrcpy_parse_args(struct scrcpy_cli_args *args, int argc, char *argv[]);

#ifdef SC_TEST
bool
sc_parse_shortcut_mods(const char *s, uint8_t *shortcut_mods);
#endif

#endif
