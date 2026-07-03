#ifndef SC_CONTROL_EXEC_H
#define SC_CONTROL_EXEC_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "controller.h"
#include "coords.h"

/**
 * Execute --control command arguments.
 *
 * Each item of `cmds` is one --control argument using the control
 * mini-language ("click <x> <y> [ms]", "swipe <x1> <y1> <x2> <y2> [ms]",
 * "input <text>", "sleep <ms>", steps chained with "&&").
 *
 * Multiple arguments without "&&" are executed with the multi-touch parallel
 * model; if any argument contains "&&", each argument runs in its own thread
 * (parallel between args, serial within an arg).
 *
 * Touch pointer ids are allocated from `pointer_id_base` (one id per
 * argument). Callers running concurrently must pass disjoint ranges.
 *
 * `screen_size` is the reference frame of the click/swipe coordinates and
 * must match what the device-side PositionMapper expects:
 *  - when the session captures video, pass the actual video size (the
 *    server IGNORES events whose screen_size differs from the video size),
 *    with coordinates in video-frame (screenshot) space;
 *  - when video is disabled, the server uses raw device coordinates and
 *    ignores screen_size; pass {UINT16_MAX, UINT16_MAX}.
 *
 * This function blocks until all commands complete.
 */
bool
sc_control_exec_run(struct sc_controller *controller,
                    struct sc_size screen_size,
                    const char *const *cmds, unsigned count,
                    uint64_t pointer_id_base);

#endif
