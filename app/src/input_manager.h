#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <stdbool.h>

#include "config.h"
#include "common.h"
#include "controller.h"
#include "fps_counter.h"
#include "video_buffer.h"
#include "screen.h"

struct input_manager {
    struct controller *controller;
    struct video_buffer *video_buffer;
    struct screen *screen;
    bool prefer_text;
};

void
input_manager_process_text_input(struct input_manager *im,
                                 const SDL_TextInputEvent *event);

void
input_manager_process_key(struct input_manager *im,
                          const SDL_KeyboardEvent *event,
                          bool control);

void
input_manager_process_mouse_motion(struct input_manager *im,
                                   const SDL_MouseMotionEvent *event);

void
input_manager_process_touch(struct input_manager *im,
                            const SDL_TouchFingerEvent *event);

void
input_manager_process_mouse_button(struct input_manager *im,
                                   const SDL_MouseButtonEvent *event,
                                   bool control);

void
input_manager_process_mouse_wheel(struct input_manager *im,
                                  const SDL_MouseWheelEvent *event);

#endif
