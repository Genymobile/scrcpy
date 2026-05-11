#ifndef SC_TERM_H
#define SC_TERM_H

#include "common.h"

#include <stdbool.h>

/**
 * Return the terminal dimensions
 *
 * Return false if the dimensions could not be retrieved.
 *
 * Otherwise, return true, and:
 *  - if `rows` is not NULL, then the number of rows is written to `*rows`.
 *  - if `columns` is not NULL, then the number of columns is written to
 *    `*columns`.
 */
bool
sc_term_get_size(unsigned *rows, unsigned *cols);

/**
 * Set the terminal tab/window title.
 *
 * Emits an OSC 0 escape sequence (\033]0;<title>\007) recognized by Windows
 * Terminal, macOS Terminal, xterm, and most modern terminal emulators.
 * On Windows, also calls SetConsoleTitleA() as a fallback for legacy hosts
 * (cmd.exe, conhost) that do not process VT sequences.
 *
 * Pass an empty string ("") to clear the title and let the shell restore its
 * own title on the next prompt.
 */
void
sc_term_set_title(const char *title);

#endif
