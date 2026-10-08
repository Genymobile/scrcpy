#include "keyboard_shortcuts.h"

#include <assert.h>

bool
sc_keyboard_shortcut_to_scroll(SDL_Keycode keycode, bool shift,
                               float *hscroll, float *vscroll) {
    assert(hscroll);
    assert(vscroll);

    if (shift) {
        return false;
    }

    *hscroll = 0.0f;

    switch (keycode) {
        case SDLK_PAGEUP:
            *vscroll = 1.0f;
            return true;
        case SDLK_PAGEDOWN:
            *vscroll = -1.0f;
            return true;
        default:
            return false;
    }
}
