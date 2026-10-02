#ifndef SC_TOOLBAR_PREFERENCES_H
#define SC_TOOLBAR_PREFERENCES_H

#include "common.h"

#include <stdbool.h>

bool
sc_toolbar_preferences_load_visible(void);

bool
sc_toolbar_preferences_save_visible(bool visible);

#endif
