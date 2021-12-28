#include <input_events.h>

static inline enum sc_mod
sc_mod_from_sdl(SDL_Keymod mod) {
    return (enum sc_mod) mod;
}

static inline enum sc_keycode
sc_keycode_from_sdl(SDL_Keycode keycode) {
    return (enum sc_keycode) keycode;
}

static inline enum sc_scancode
sc_scancode_from_sdl(SDL_Scancode scancode) {
    return (enum sc_scancode) scancode;
}

static inline enum sc_action
sc_action_from_sdl_keyboard_type(uint32_t type) {
    assert(type == SDL_KEYDOWN || type == SDL_KEYUP);
    if (type == SDL_KEYDOWN) {
        return SC_ACTION_DOWN;
    }
    return SC_ACTION_UP;
}

static inline enum sc_action
sc_action_from_sdl_mousebutton_type(uint32_t type) {
    assert(type == SDL_MOUSEBUTTONDOWN || type == SDL_MOUSEBUTTONUP);
    if (type == SDL_MOUSEBUTTONDOWN) {
        return SC_ACTION_DOWN;
    }
    return SC_ACTION_UP;
}

void
sc_key_event_from_sdl(struct sc_key_event *event,
                      const SDL_KeyboardEvent *sdl) {
    event->action = sc_action_from_sdl_keyboard_type(sdl->type);
    event->keycode = sc_keycode_from_sdl(sdl->keysym.sym);
    event->scancode = sc_scancode_from_sdl(sdl->keysym.scancode);
    event->repeat = sdl->repeat;
    event->mods = sc_mod_from_sdl(sdl->keysym.mod);
}

void
sc_text_event_from_sdl(struct sc_text_event *event,
                       const SDL_TextInputEvent *sdl) {
    event->text = sdl->text;
}
