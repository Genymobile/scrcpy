#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "config.h"
#include "common.h"
#include "controller.h"
#include "fps_counter.h"
#include "scrcpy.h"
#include "screen.h"
#include "video_buffer.h"

struct input_manager {
    struct controller *controller;
    struct video_buffer *video_buffer;
    struct screen *screen;

    // SDL reports repeated events as a boolean, but Android expects the actual
    // number of repetitions. This variable keeps track of the count.
    unsigned repeat;

    bool control;
    bool forward_key_repeat;
    bool prefer_text;
    bool forward_all_clicks;
    bool legacy_paste;

    struct {
        unsigned data[SC_MAX_SHORTCUT_MODS];
        unsigned count;
    } sdl_shortcut_mods;

    bool vfinger_down;
};

void
input_manager_init(struct input_manager *im,
                   const struct scrcpy_options *options);

void
input_manager_process_text_input(struct input_manager *im,
                                 const SDL_TextInputEvent *event);

void
input_manager_process_key(struct input_manager *im,
                          const SDL_KeyboardEvent *event);

void
input_manager_process_mouse_motion(struct input_manager *im,
                                   const SDL_MouseMotionEvent *event);

void
input_manager_process_touch(struct input_manager *im,
                            const SDL_TouchFingerEvent *event);

void
input_manager_process_mouse_button(struct input_manager *im,
                                   const SDL_MouseButtonEvent *event);

void
input_manager_process_mouse_wheel(struct input_manager *im,
                                  const SDL_MouseWheelEvent *event);

#endif
