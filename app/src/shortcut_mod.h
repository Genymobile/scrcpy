#ifndef SC_SHORTCUT_MOD_H
#define SC_SHORTCUT_MOD_H

#include "common.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL_keycode.h>

#include "options.h"

#define SC_SDL_SHORTCUT_MODS_MASK (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)

// input: OR of enum sc_shortcut_mod
// output: OR of SDL_Keymod
static inline uint16_t
sc_shortcut_mods_to_sdl(uint8_t shortcut_mods) {
    uint16_t sdl_mod = 0;
    if (shortcut_mods & SC_SHORTCUT_MOD_LCTRL) {
        sdl_mod |= SDL_KMOD_LCTRL;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_RCTRL) {
        sdl_mod |= SDL_KMOD_RCTRL;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_LALT) {
        sdl_mod |= SDL_KMOD_LALT;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_RALT) {
        sdl_mod |= SDL_KMOD_RALT;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_LSUPER) {
        sdl_mod |= SDL_KMOD_LGUI;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_RSUPER) {
        sdl_mod |= SDL_KMOD_RGUI;
    }
    return sdl_mod;
}

static inline bool
sc_shortcut_mods_is_shortcut_mod(uint16_t sdl_shortcut_mods, uint16_t sdl_mod) {
    // sdl_shortcut_mods must be within the mask
    assert(!(sdl_shortcut_mods & ~SC_SDL_SHORTCUT_MODS_MASK));

    // at least one shortcut mod pressed?
    return sdl_mod & sdl_shortcut_mods;
}

static inline bool
sc_shortcut_mods_is_shortcut_key(uint16_t sdl_shortcut_mods,
                                 SDL_Keycode keycode) {
    return (sdl_shortcut_mods & SDL_KMOD_LCTRL && keycode == SDLK_LCTRL)
        || (sdl_shortcut_mods & SDL_KMOD_RCTRL && keycode == SDLK_RCTRL)
        || (sdl_shortcut_mods & SDL_KMOD_LALT  && keycode == SDLK_LALT)
        || (sdl_shortcut_mods & SDL_KMOD_RALT  && keycode == SDLK_RALT)
        || (sdl_shortcut_mods & SDL_KMOD_LGUI  && keycode == SDLK_LGUI)
        || (sdl_shortcut_mods & SDL_KMOD_RGUI  && keycode == SDLK_RGUI);
}

#endif
