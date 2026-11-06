#ifndef SC_MACOS_MENU_H
#define SC_MACOS_MENU_H

#include "common.h"

#include <stdbool.h>

struct sc_screen;

// Called on the SDL main thread, after creating the screen.
bool
sc_macos_menu_init(struct sc_screen *screen);

void
sc_macos_menu_destroy(void);

#endif
