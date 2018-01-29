#ifndef SCREEN_H
#define SCREEN_H

#include <SDL2/SDL_stdinc.h>

SDL_bool scrcpy(const char *serial, Uint16 local_port, Uint16 maximum_size);

#endif
