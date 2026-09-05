#ifndef SC_DAEMON_MIRROR_H
#define SC_DAEMON_MIRROR_H

#include "common.h"

#include "options.h"
#include "scrcpy.h"

/**
 * Daemon mirror mode: `scrcpy-auto --client-port PORT` with no operation.
 *
 * Behaves like plain scrcpy (SDL window, rendering, mouse/keyboard input),
 * except that the video comes from a running daemon's `subscribe_video`
 * stream (doc/daemon.md §8.7) instead of a new device session, and input is
 * forwarded as daemon `inject_*` requests on a second connection. Works
 * against the real daemon and against protocol emulators such as the iOS
 * bridge (which rejects input with a clean error — view-only there).
 */
enum scrcpy_exit_code
sc_mirror_run(const struct scrcpy_options *opts);

#endif
