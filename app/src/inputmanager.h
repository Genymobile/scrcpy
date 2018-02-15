#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "common.h"
#include "controller.h"
#include "fpscounter.h"
#include "frames.h"
#include "screen.h"

struct input_manager {
    struct controller *controller;
    struct frames *frames;
    struct screen *screen;
};

void input_manager_process_text_input(struct input_manager *input_manager,
                                      const SDL_TextInputEvent *event);
void input_manager_process_key(struct input_manager *input_manager,
                               const SDL_KeyboardEvent *event);
void input_manager_process_mouse_motion(struct input_manager *input_manager,
                                        const SDL_MouseMotionEvent *event);
void input_manager_process_mouse_button(struct input_manager *input_manager,
                                        const SDL_MouseButtonEvent *event);
void input_manager_process_mouse_wheel(struct input_manager *input_manager,
                                       const SDL_MouseWheelEvent *event);

#endif
