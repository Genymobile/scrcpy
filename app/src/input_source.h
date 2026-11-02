#ifndef SC_INPUT_SOURCE_H
#define SC_INPUT_SOURCE_H

#include "common.h"

#include <stdbool.h>

/**
 * Tell whether the current computer keyboard input source is an input method
 * (e.g. 2-Set Korean, Japanese Kana) rather than a plain keyboard layout (e.g.
 * ABC, U.S.).
 *
 * Return false if this information is not available on this platform.
 */
bool
sc_input_source_is_input_method(bool *input_method);

#endif
