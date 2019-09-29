#ifndef CONVERT_H
#define CONVERT_H

#include <stdbool.h>
#include <SDL2/SDL_events.h>

#include "config.h"
#include "control_msg.h"

struct complete_mouse_motion_event {
    SDL_MouseMotionEvent *mouse_motion_event;
    struct size screen_size;
};

struct complete_mouse_wheel_event {
    SDL_MouseWheelEvent *mouse_wheel_event;
    struct point position;
};

bool
convert_input_key(const SDL_KeyboardEvent *from, struct control_msg *to);

bool
convert_mouse_button(const SDL_MouseButtonEvent *from, struct size screen_size,
                     struct control_msg *to);

// the video size may be different from the real device size, so we need the
// size to which the absolute position apply, to scale it accordingly
bool
convert_mouse_motion(const SDL_MouseMotionEvent *from, struct size screen_size,
                     struct control_msg *to);

// on Android, a scroll event requires the current mouse position
bool
convert_mouse_wheel(const SDL_MouseWheelEvent *from, struct position position,
                    struct control_msg *to);

#endif
