#ifndef SC_KEYBOARD_SHORTCUTS_H
#define SC_KEYBOARD_SHORTCUTS_H

#include <stdbool.h>
#include <SDL3/SDL_keycode.h>

bool
sc_keyboard_shortcut_to_scroll(SDL_Keycode keycode, bool shift,
                               float *hscroll, float *vscroll);

#endif
