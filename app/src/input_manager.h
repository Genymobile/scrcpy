#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "common.h"

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "controller.h"
#include "fps_counter.h"
#include "options.h"
#include "screen.h"
#include "trait/key_processor.h"
#include "trait/mouse_processor.h"

struct input_manager {
    struct controller *controller;
    struct screen *screen;

    struct sc_key_processor *kp;
    struct sc_mouse_processor *mp;

    bool control;
    bool forward_all_clicks;
    bool legacy_paste;

    struct {
        unsigned data[SC_MAX_SHORTCUT_MODS];
        unsigned count;
    } sdl_shortcut_mods;

    bool vfinger_down;

    // Tracks the number of identical consecutive shortcut key down events.
    // Not to be confused with event->repeat, which counts the number of
    // system-generated repeated key presses.
    unsigned key_repeat;
    SDL_Keycode last_keycode;
    uint16_t last_mod;
};

void
input_manager_init(struct input_manager *im, struct controller *controller,
                   struct screen *screen, struct sc_key_processor *kp,
                   struct sc_mouse_processor *mp,
                   const struct scrcpy_options *options);

bool
input_manager_handle_event(struct input_manager *im, SDL_Event *event);

#endif
