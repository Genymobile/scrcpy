#ifndef SCRCPY_H
#define SCRCPY_H

#include <SDL2/SDL_stdinc.h>

SDL_bool scrcpy(const char *serial, Uint16 local_port, Uint16 max_size, Uint32 bit_rate);

#endif
