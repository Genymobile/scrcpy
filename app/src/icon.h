#ifndef SC_ICON_H
#define SC_ICON_H

#include "common.h"

#include <SDL3/SDL_surface.h>

#define SC_ICON_FILENAME_SCRCPY "scrcpy.png"
#define SC_ICON_FILENAME_DISCONNECTED "disconnected.png"

SDL_Surface *
sc_icon_load(const char *filename);

void
sc_icon_destroy(SDL_Surface *icon);

#endif
