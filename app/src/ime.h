#ifndef SC_IME_H
#define SC_IME_H

#include "common.h"

#include <stdbool.h>

#include "util/intr.h"

#define SC_IME_PACKAGE_NAME "com.genymobile.scrcpy.ime"

bool
sc_ime_prepare_device(struct sc_intr *intr, const char *serial);

#endif
