#ifndef CONVERT_H
#define CONVERT_H

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_events.h>
#include "controlevent.h"

// on Android, a scroll event requires the current mouse position
struct complete_mouse_wheel_event {
    SDL_MouseWheelEvent *mouse_wheel_event;
    Sint32 x;
    Sint32 y;
};

SDL_bool input_key_from_sdl_to_android(SDL_KeyboardEvent *from, struct control_event *to);
SDL_bool mouse_button_from_sdl_to_android(SDL_MouseButtonEvent *from, struct control_event *to);
SDL_bool mouse_motion_from_sdl_to_android(SDL_MouseMotionEvent *from, struct control_event *to);
SDL_bool mouse_wheel_from_sdl_to_android(struct complete_mouse_wheel_event *from, struct control_event *to);

#endif
