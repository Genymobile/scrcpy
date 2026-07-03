#ifndef SC_CONTROL_EXEC_H
#define SC_CONTROL_EXEC_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "controller.h"

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
 * This function blocks until all commands complete.
 */
bool
sc_control_exec_run(struct sc_controller *controller,
                    const char *const *cmds, unsigned count,
                    uint64_t pointer_id_base);

#endif
