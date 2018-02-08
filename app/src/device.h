#ifndef DEVICE_H
#define DEVICE_H

#include <SDL2/SDL_net.h>
#include <SDL2/SDL_stdinc.h>

#include "common.h"

#define DEVICE_NAME_FIELD_LENGTH 64

// name must be at least DEVICE_NAME_FIELD_LENGTH bytes
SDL_bool device_read_info(TCPsocket device_socket, char *name, struct size *frame_size);

#endif
