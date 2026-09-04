#ifndef SC_SDL_HINTS_H
#define SC_SDL_HINTS_H

#include "common.h"

#include <SDL3/SDL_hints.h>

void
sc_sdl_set_hints(const char *render_driver, bool disable_screensaver);

#endif

