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
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            sc_mouse_capture_set_active(mc, false);
            return true;
        case SDL_EVENT_KEY_DOWN: {
            SDL_Keycode key = event->key.key;
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
        case SDL_EVENT_KEY_UP: {
            SDL_Keycode key = event->key.key;
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
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!sc_mouse_capture_is_active(mc)) {
                // The mouse will be captured on SDL_EVENT_MOUSE_BUTTON_UP, so
                // consume the event
                return true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (!sc_mouse_capture_is_active(mc)) {
                sc_mouse_capture_set_active(mc, true);
                return true;
            }
            break;
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
            // Touch events are not compatible with relative mode
            // (coordinates are not relative), so consume the event
            return true;
    }

    return false;
}

void
sc_mouse_capture_set_active(struct sc_mouse_capture *mc, bool capture) {
    bool ok = SDL_SetWindowRelativeMouseMode(mc->window, capture);
    if (!ok) {
        LOGE("Could not set relative mouse mode to %s: %s",
             capture ? "true" : "false", SDL_GetError());
    }
}

bool
sc_mouse_capture_is_active(struct sc_mouse_capture *mc) {
    return SDL_GetWindowRelativeMouseMode(mc->window);
}

void
sc_mouse_capture_toggle(struct sc_mouse_capture *mc) {
    bool new_value = !sc_mouse_capture_is_active(mc);
    sc_mouse_capture_set_active(mc, new_value);
}
