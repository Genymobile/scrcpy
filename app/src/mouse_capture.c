#include "mouse_capture.h"

#include "shortcut_mod.h"
#include "util/log.h"

void
sc_mouse_capture_init(struct sc_mouse_capture *mc, SDL_Window *window,
                      uint8_t shortcut_mods) {
    mc->window = window;
    mc->sdl_mouse_capture_keys = sc_shortcut_mods_to_sdl(shortcut_mods);
    mc->mouse_capture_key_pressed = SDLK_UNKNOWN;
}

static inline bool
sc_mouse_capture_is_capture_key(struct sc_mouse_capture *mc, SDL_Keycode key) {
    return sc_shortcut_mods_is_shortcut_key(mc->sdl_mouse_capture_keys, key);
}

bool
sc_mouse_capture_handle_event(struct sc_mouse_capture *mc,
                              const SDL_Event *event) {
    switch (event->type) {
        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                sc_mouse_capture_set_active(mc, false);
                return true;
            }
            break;
        case SDL_KEYDOWN: {
            SDL_Keycode key = event->key.keysym.sym;
            if (sc_mouse_capture_is_capture_key(mc, key)) {
                if (!mc->mouse_capture_key_pressed) {
                    mc->mouse_capture_key_pressed = key;
                } else {
                    // Another mouse capture key has been pressed, cancel
                    // mouse (un)capture
                    mc->mouse_capture_key_pressed = 0;
                }
                // Mouse capture keys are never forwarded to the device
                return true;
            }
            break;
        }
        case SDL_KEYUP: {
            SDL_Keycode key = event->key.keysym.sym;
            SDL_Keycode cap = mc->mouse_capture_key_pressed;
            mc->mouse_capture_key_pressed = 0;
            if (sc_mouse_capture_is_capture_key(mc, key)) {
                if (key == cap) {
                    // A mouse capture key has been pressed then released:
                    // toggle the capture mouse mode
                    sc_mouse_capture_toggle(mc);
                }
                // Mouse capture keys are never forwarded to the device
                return true;
            }
            break;
        }
        case SDL_MOUSEWHEEL:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
            if (!sc_mouse_capture_is_active(mc)) {
                // The mouse will be captured on SDL_MOUSEBUTTONUP, so consume
                // the event
                return true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (!sc_mouse_capture_is_active(mc)) {
                sc_mouse_capture_set_active(mc, true);
                return true;
            }
            break;
        case SDL_FINGERMOTION:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
            // Touch events are not compatible with relative mode
            // (coordinates are not relative), so consume the event
            return true;
    }

    return false;
}

void
sc_mouse_capture_set_active(struct sc_mouse_capture *mc, bool capture) {
#ifdef __APPLE__
    // Workaround for SDL bug on macOS:
    // <https://github.com/libsdl-org/SDL/issues/5340>
    if (capture) {
        int mouse_x, mouse_y;
        SDL_GetGlobalMouseState(&mouse_x, &mouse_y);

        int x, y, w, h;
        SDL_GetWindowPosition(mc->window, &x, &y);
        SDL_GetWindowSize(mc->window, &w, &h);

        bool outside_window = mouse_x < x || mouse_x >= x + w
                           || mouse_y < y || mouse_y >= y + h;
        if (outside_window) {
            SDL_WarpMouseInWindow(mc->window, w / 2, h / 2);
        }
    }
#else
    (void) mc;
#endif
    if (SDL_SetRelativeMouseMode(capture)) {
        LOGE("Could not set relative mouse mode to %s: %s",
             capture ? "true" : "false", SDL_GetError());
    }
}

bool
sc_mouse_capture_is_active(struct sc_mouse_capture *mc) {
    (void) mc;
    return SDL_GetRelativeMouseMode();
}

void
sc_mouse_capture_toggle(struct sc_mouse_capture *mc) {
    bool new_value = !sc_mouse_capture_is_active(mc);
    sc_mouse_capture_set_active(mc, new_value);
}
