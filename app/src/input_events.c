#include <input_events.h>

static inline uint16_t
sc_mods_state_from_sdl(uint16_t mods_state) {
    return mods_state;
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

static inline enum sc_mouse_button
sc_mouse_button_from_sdl(uint8_t button) {
    if (button >= SDL_BUTTON_LEFT && button <= SDL_BUTTON_X2) {
        // SC_MOUSE_BUTTON_* constants are initialized from SDL_BUTTON(index)
        return SDL_BUTTON(button);
    }

    return SC_MOUSE_BUTTON_UNKNOWN;
}

static inline uint8_t
sc_mouse_buttons_state_from_sdl(uint32_t buttons_state) {
    assert(buttons_state < 0x100); // fits in uint8_t
    return buttons_state;
}

void
sc_key_event_from_sdl(struct sc_key_event *event,
                      const SDL_KeyboardEvent *sdl) {
    event->action = sc_action_from_sdl_keyboard_type(sdl->type);
    event->keycode = sc_keycode_from_sdl(sdl->keysym.sym);
    event->scancode = sc_scancode_from_sdl(sdl->keysym.scancode);
    event->repeat = sdl->repeat;
    event->mods_state = sc_mods_state_from_sdl(sdl->keysym.mod);
}

void
sc_text_event_from_sdl(struct sc_text_event *event,
                       const SDL_TextInputEvent *sdl) {
    event->text = sdl->text;
}

void
sc_mouse_click_event_from_sdl(struct sc_mouse_click_event *event,
                              const struct SDL_MouseButtonEvent *sdl) {
    event->action = sc_action_from_sdl_mousebutton_type(sdl->type);
    event->button = sc_mouse_button_from_sdl(sdl->button);
}
