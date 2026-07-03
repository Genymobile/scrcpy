#ifndef SC_DAEMON_CLIENT_H
#define SC_DAEMON_CLIENT_H

#include "common.h"

#include "options.h"
#include "scrcpy.h"

/**
 * Run in thin client mode (doc/daemon.md §8.6): connect to the daemon on
 * 127.0.0.1:opts->client_port and execute the requested operations in order:
 * control, screencap, status, shutdown.
 *
 * Exit codes: SCRCPY_EXIT_SUCCESS on success, SCRCPY_EXIT_FAILURE if a
 * request failed daemon-side, SCRCPY_EXIT_DISCONNECTED (2) if the daemon
 * could not be reached or the protocol versions do not match.
 */
enum scrcpy_exit_code
sc_client_run(const struct scrcpy_options *opts);

#endif
