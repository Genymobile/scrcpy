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

#endif
