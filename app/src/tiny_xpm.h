#ifndef TINYXPM_H
#define TINYXPM_H

#include <SDL2/SDL.h>

#include "common.h"

SDL_Surface *
read_xpm(char *xpm[]);

#endif
