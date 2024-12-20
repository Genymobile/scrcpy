#ifndef SC_INPUTMANAGER_H
#define SC_INPUTMANAGER_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>

#include "controller.h"
#include "file_pusher.h"
#include "options.h"
#include "trait/gamepad_processor.h"
#include "trait/key_processor.h"
#include "trait/mouse_processor.h"

struct sc_input_manager {
    struct sc_controller *controller;
    struct sc_file_pusher *fp;
    struct sc_screen *screen;

    struct sc_key_processor *kp;
    struct sc_mouse_processor *mp;
    struct sc_gamepad_processor *gp;

    struct sc_mouse_bindings mouse_bindings;
    bool legacy_paste;
    bool clipboard_autosync;

    uint16_t sdl_shortcut_mods;

    bool vfinger_down;
    bool vfinger_invert_x;
    bool vfinger_invert_y;

    uint8_t mouse_buttons_state; // OR of enum sc_mouse_button values

    // Tracks the number of identical consecutive shortcut key down events.
    // Not to be confused with event->repeat, which counts the number of
    // system-generated repeated key presses.
    unsigned key_repeat;
    SDL_Keycode last_keycode;
    uint16_t last_mod;

    uint64_t next_sequence; // used for request acknowledgements
};

struct sc_input_manager_params {
    struct sc_controller *controller;
    struct sc_file_pusher *fp;
    struct sc_screen *screen;
    struct sc_key_processor *kp;
    struct sc_mouse_processor *mp;
    struct sc_gamepad_processor *gp;

    struct sc_mouse_bindings mouse_bindings;
    bool legacy_paste;
    bool clipboard_autosync;
    uint8_t shortcut_mods; // OR of enum sc_shortcut_mod values
};

void
sc_input_manager_init(struct sc_input_manager *im,
                      const struct sc_input_manager_params *params);

void
sc_input_manager_handle_event(struct sc_input_manager *im,
                              const SDL_Event *event);

#endif
