#ifndef SC_CAMERA_H
#define SC_CAMERA_H

#include "input_manager.h"

#include <SDL2/SDL_events.h>

void sc_input_manager_handle_event_camera(struct sc_input_manager *im,
                                          const SDL_Event *event);

#endif
