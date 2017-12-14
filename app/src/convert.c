#include "convert.h"

#define MAP(FROM, TO) case FROM: *to = TO; return SDL_TRUE
#define FAIL default: return SDL_FALSE

static SDL_bool convert_keycode_action(SDL_EventType from, enum android_keyevent_action *to) {
    switch (from) {
        MAP(SDL_KEYDOWN, AKEY_EVENT_ACTION_DOWN);
        MAP(SDL_KEYUP,   AKEY_EVENT_ACTION_UP);
        FAIL;
    }
}

static enum android_metastate autocomplete_metastate(enum android_metastate metastate) {
    // fill dependant flags
    if (metastate & (AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_RIGHT_ON)) {
        metastate |= AMETA_SHIFT_ON;
    }
    if (metastate & (AMETA_CTRL_LEFT_ON | AMETA_CTRL_RIGHT_ON)) {
        metastate |= AMETA_CTRL_ON;
    }
    if (metastate & (AMETA_ALT_LEFT_ON | AMETA_ALT_RIGHT_ON)) {
        metastate |= AMETA_ALT_ON;
    }
    if (metastate & (AMETA_META_LEFT_ON | AMETA_META_RIGHT_ON)) {
        metastate |= AMETA_META_ON;
    }

    return metastate;
}


static enum android_metastate convert_meta_state(SDL_Keymod mod) {
    enum android_metastate metastate = 0;
    if (mod & KMOD_LSHIFT) {
        metastate |= AMETA_SHIFT_LEFT_ON;
    }
    if (mod & KMOD_RSHIFT) {
        metastate |= AMETA_SHIFT_RIGHT_ON;
    }
    if (mod & KMOD_LCTRL) {
        metastate |= AMETA_CTRL_LEFT_ON;
    }
    if (mod & KMOD_RCTRL) {
        metastate |= AMETA_CTRL_RIGHT_ON;
    }
    if (mod & KMOD_LALT) {
        metastate |= AMETA_ALT_LEFT_ON;
    }
    if (mod & KMOD_RALT) {
        metastate |= AMETA_ALT_RIGHT_ON;
    }
    if (mod & KMOD_LGUI) { // Windows key
        metastate |= AMETA_META_LEFT_ON;
    }
    if (mod & KMOD_RGUI) { // Windows key
        metastate |= AMETA_META_RIGHT_ON;
    }
    if (mod & KMOD_NUM) {
        metastate |= AMETA_NUM_LOCK_ON;
    }
    if (mod & KMOD_CAPS) {
        metastate |= AMETA_CAPS_LOCK_ON;
    }
    if (mod & KMOD_MODE) { // Alt Gr
        // no mapping?
    }

    // fill the dependent fields
    return autocomplete_metastate(metastate);
}

static SDL_bool convert_keycode(SDL_Keycode from, enum android_keycode *to) {
    switch (from) {
        MAP(SDLK_LCTRL,        AKEYCODE_CTRL_LEFT);
        MAP(SDLK_LSHIFT,       AKEYCODE_SHIFT_LEFT);
        MAP(SDLK_LALT,         AKEYCODE_ALT_LEFT);
        MAP(SDLK_LGUI,         AKEYCODE_META_LEFT);
        MAP(SDLK_RCTRL,        AKEYCODE_CTRL_RIGHT);
        MAP(SDLK_RSHIFT,       AKEYCODE_SHIFT_RIGHT);
        MAP(SDLK_RALT,         AKEYCODE_ALT_RIGHT);
        MAP(SDLK_RGUI,         AKEYCODE_META_RIGHT);
        MAP(SDLK_RETURN,       AKEYCODE_ENTER);
        MAP(SDLK_ESCAPE,       AKEYCODE_ESCAPE);
        MAP(SDLK_BACKSPACE,    AKEYCODE_DEL);
        MAP(SDLK_TAB,          AKEYCODE_TAB);
        MAP(SDLK_SPACE,        AKEYCODE_SPACE);
        MAP(SDLK_F1,           AKEYCODE_F1);
        MAP(SDLK_F2,           AKEYCODE_F2);
        MAP(SDLK_F3,           AKEYCODE_F3);
        MAP(SDLK_F4,           AKEYCODE_F4);
        MAP(SDLK_F5,           AKEYCODE_F5);
        MAP(SDLK_F6,           AKEYCODE_F6);
        MAP(SDLK_F7,           AKEYCODE_F7);
        MAP(SDLK_F8,           AKEYCODE_F8);
        MAP(SDLK_F9,           AKEYCODE_F9);
        MAP(SDLK_F10,          AKEYCODE_F10);
        MAP(SDLK_F11,          AKEYCODE_F11);
        MAP(SDLK_F12,          AKEYCODE_F12);
        MAP(SDLK_PRINTSCREEN,  AKEYCODE_SYSRQ);
        MAP(SDLK_SCROLLLOCK,   AKEYCODE_SCROLL_LOCK);
        MAP(SDLK_PAUSE,        AKEYCODE_BREAK);
        MAP(SDLK_INSERT,       AKEYCODE_INSERT);
        MAP(SDLK_HOME,         AKEYCODE_HOME);
        MAP(SDLK_PAGEUP,       AKEYCODE_PAGE_UP);
        MAP(SDLK_DELETE,       AKEYCODE_FORWARD_DEL);
        MAP(SDLK_END,          AKEYCODE_MOVE_END);
        MAP(SDLK_PAGEDOWN,     AKEYCODE_PAGE_DOWN);
        MAP(SDLK_RIGHT,        AKEYCODE_DPAD_RIGHT);
        MAP(SDLK_LEFT,         AKEYCODE_DPAD_LEFT);
        MAP(SDLK_DOWN,         AKEYCODE_DPAD_DOWN);
        MAP(SDLK_UP,           AKEYCODE_DPAD_UP);
        MAP(SDLK_NUMLOCKCLEAR, AKEYCODE_NUM_LOCK);
        MAP(SDLK_KP_DIVIDE,    AKEYCODE_NUMPAD_DIVIDE);
        MAP(SDLK_KP_MULTIPLY,  AKEYCODE_NUMPAD_MULTIPLY);
        MAP(SDLK_KP_MINUS,     AKEYCODE_NUMPAD_SUBTRACT);
        MAP(SDLK_KP_PLUS,      AKEYCODE_NUMPAD_ADD);
        MAP(SDLK_KP_ENTER,     AKEYCODE_NUMPAD_ENTER);
        MAP(SDLK_KP_1,         AKEYCODE_NUMPAD_1);
        MAP(SDLK_KP_2,         AKEYCODE_NUMPAD_2);
        MAP(SDLK_KP_3,         AKEYCODE_NUMPAD_3);
        MAP(SDLK_KP_4,         AKEYCODE_NUMPAD_4);
        MAP(SDLK_KP_5,         AKEYCODE_NUMPAD_5);
        MAP(SDLK_KP_6,         AKEYCODE_NUMPAD_6);
        MAP(SDLK_KP_7,         AKEYCODE_NUMPAD_7);
        MAP(SDLK_KP_8,         AKEYCODE_NUMPAD_8);
        MAP(SDLK_KP_9,         AKEYCODE_NUMPAD_9);
        MAP(SDLK_KP_0,         AKEYCODE_NUMPAD_0);
        MAP(SDLK_KP_PERIOD,    AKEYCODE_NUMPAD_DOT);
        FAIL;
    }
}

static SDL_bool convert_mouse_action(SDL_EventType from, enum android_motionevent_action *to) {
    switch (from) {
        MAP(SDL_MOUSEBUTTONDOWN, AMOTION_EVENT_ACTION_DOWN);
        MAP(SDL_MOUSEBUTTONUP,   AMOTION_EVENT_ACTION_UP);
        FAIL;
    }
}

static enum android_motionevent_buttons convert_mouse_buttons(Uint32 state) {
    enum android_motionevent_buttons buttons = 0;
    if (state & SDL_BUTTON_LMASK) {
        buttons |= AMOTION_EVENT_BUTTON_PRIMARY;
    }
    if (state & SDL_BUTTON_RMASK) {
        buttons |= AMOTION_EVENT_BUTTON_SECONDARY;
    }
    if (state & SDL_BUTTON_MMASK) {
        buttons |= AMOTION_EVENT_BUTTON_TERTIARY;
    }
    if (state & SDL_BUTTON_X1) {
        buttons |= AMOTION_EVENT_BUTTON_BACK;
    }
    if (state & SDL_BUTTON_X2) {
        buttons |= AMOTION_EVENT_BUTTON_FORWARD;
    }
    return buttons;
}

SDL_bool input_key_from_sdl_to_android(SDL_KeyboardEvent *from, struct control_event *to) {
    to->type = CONTROL_EVENT_TYPE_KEYCODE;

    if (!convert_keycode_action(from->type, &to->keycode_event.action)) {
        return SDL_FALSE;
    }

    if (!convert_keycode(from->keysym.sym, &to->keycode_event.keycode)) {
        return SDL_FALSE;
    }

    to->keycode_event.metastate = convert_meta_state(from->keysym.mod);

    return SDL_TRUE;
}

SDL_bool mouse_button_from_sdl_to_android(SDL_MouseButtonEvent *from, struct control_event *to) {
    to->type = CONTROL_EVENT_TYPE_MOUSE;

    if (!convert_mouse_action(from->type, &to->mouse_event.action)) {
        return SDL_FALSE;
    }

    to->mouse_event.buttons = convert_mouse_buttons(SDL_BUTTON(from->button));
    to->mouse_event.x = from->x;
    to->mouse_event.y = from->y;

    return SDL_TRUE;
}

SDL_bool mouse_motion_from_sdl_to_android(SDL_MouseMotionEvent *from, struct control_event *to) {
    to->type = CONTROL_EVENT_TYPE_MOUSE;
    to->mouse_event.action = AMOTION_EVENT_ACTION_MOVE;
    to->mouse_event.buttons = convert_mouse_buttons(from->state);
    to->mouse_event.x = from->x;
    to->mouse_event.y = from->y;

    return SDL_TRUE;
}

SDL_bool mouse_wheel_from_sdl_to_android(struct complete_mouse_wheel_event *from, struct control_event *to) {
    to->type = CONTROL_EVENT_TYPE_SCROLL;

    to->scroll_event.x = from->x;
    to->scroll_event.y = from->y;

    SDL_MouseWheelEvent *wheel = from->mouse_wheel_event;
    int mul = wheel->direction == SDL_MOUSEWHEEL_NORMAL ? 1 : -1;
    to->scroll_event.hscroll = mul * wheel->x;
    to->scroll_event.vscroll = mul * wheel->y;

    return SDL_TRUE;
}
