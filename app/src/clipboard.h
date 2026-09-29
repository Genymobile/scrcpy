#ifndef SC_CLIPBOARD_H
#define SC_CLIPBOARD_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

// Encode the surface as PNG and publish it through the system clipboard.
bool
sc_clipboard_set_png(SDL_Surface *surface);

#endif
