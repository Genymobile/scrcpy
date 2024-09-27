#ifndef SC_SHORTCUT_MOD_H
#define SC_SHORTCUT_MOD_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_keycode.h>

#include "options.h"

#define SC_SDL_SHORTCUT_MODS_MASK (KMOD_CTRL | KMOD_ALT | KMOD_GUI)

// input: OR of enum sc_shortcut_mod
// output: OR of SDL_Keymod
static inline uint16_t
sc_shortcut_mods_to_sdl(uint8_t shortcut_mods) {
    uint16_t sdl_mod = 0;
    if (shortcut_mods & SC_SHORTCUT_MOD_LCTRL) {
        sdl_mod |= KMOD_LCTRL;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_RCTRL) {
        sdl_mod |= KMOD_RCTRL;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_LALT) {
        sdl_mod |= KMOD_LALT;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_RALT) {
        sdl_mod |= KMOD_RALT;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_LSUPER) {
        sdl_mod |= KMOD_LGUI;
    }
    if (shortcut_mods & SC_SHORTCUT_MOD_RSUPER) {
        sdl_mod |= KMOD_RGUI;
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
    return (sdl_shortcut_mods & KMOD_LCTRL && keycode == SDLK_LCTRL)
        || (sdl_shortcut_mods & KMOD_RCTRL && keycode == SDLK_RCTRL)
        || (sdl_shortcut_mods & KMOD_LALT  && keycode == SDLK_LALT)
        || (sdl_shortcut_mods & KMOD_RALT  && keycode == SDLK_RALT)
        || (sdl_shortcut_mods & KMOD_LGUI  && keycode == SDLK_LGUI)
        || (sdl_shortcut_mods & KMOD_RGUI  && keycode == SDLK_RGUI);
}

#endif
