#ifndef SLINK_H
#define SLINK_H

#include "common.h"

#include "options.h"

enum slink_exit_code {
    // Normal program termination
    SLINK_EXIT_SUCCESS,

    // No connection could be established
    SLINK_EXIT_FAILURE,

    // Device was disconnected while running
    SLINK_EXIT_DISCONNECTED,
};

enum slink_exit_code
slink(struct slink_options *options);

#endif
