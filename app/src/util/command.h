#ifndef SC_COMMAND_H
#define SC_COMMAND_H

#include "common.h"

/**
 * Serialize an argv array for Windows
 *
 * Convert a NULL-terminated argument array into a single escaped string
 * suitable for massing to CreateProcess() on Windows.
 *
 * The returned value must be freed by the caller.
 */
char *
sc_command_serialize_windows(const char *const argv[]);

#endif
