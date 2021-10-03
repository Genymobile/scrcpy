#ifndef CONVERT_H
#define CONVERT_H

#include "common.h"

#include <stdbool.h>
#include <SDL2/SDL_events.h>

#include "control_msg.h"

enum android_motionevent_buttons
convert_mouse_buttons(uint32_t state);

bool
convert_mouse_action(SDL_EventType from, enum android_motionevent_action *to);

bool
convert_touch_action(SDL_EventType from, enum android_motionevent_action *to);

#endif
