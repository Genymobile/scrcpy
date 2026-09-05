#ifndef SC_DAEMON_H
#define SC_DAEMON_H

#include "common.h"

#include "options.h"
#include "scrcpy.h"

/**
 * Run in daemon mode (doc/daemon.md): keep a persistent session with the
 * device and serve client requests on 127.0.0.1:opts->daemon_port.
 *
 * Blocks until stopped (signal, client shutdown request, or fatal error).
 */
enum scrcpy_exit_code
sc_daemon_run(struct scrcpy_options *opts);

#endif
