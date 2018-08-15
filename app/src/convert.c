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

// only used if raw_key_events is enabled
static SDL_bool convert_text_keycode(SDL_Keycode from, enum android_keycode *to) {
    switch (from) {
        MAP(SDLK_SPACE,         AKEYCODE_SPACE);
        MAP(SDLK_HASH,          AKEYCODE_POUND);
        MAP(SDLK_QUOTE,         AKEYCODE_APOSTROPHE);
        MAP(SDLK_ASTERISK,      AKEYCODE_STAR);
        MAP(SDLK_COMMA,         AKEYCODE_COMMA);
        MAP(SDLK_MINUS,         AKEYCODE_MINUS);
        MAP(SDLK_PERIOD,        AKEYCODE_PERIOD);
        MAP(SDLK_SLASH,         AKEYCODE_SLASH);
        MAP(SDLK_0,             AKEYCODE_0);
        MAP(SDLK_1,             AKEYCODE_1);
        MAP(SDLK_2,             AKEYCODE_2);
        MAP(SDLK_3,             AKEYCODE_3);
        MAP(SDLK_4,             AKEYCODE_4);
        MAP(SDLK_5,             AKEYCODE_5);
        MAP(SDLK_6,             AKEYCODE_6);
        MAP(SDLK_7,             AKEYCODE_7);
        MAP(SDLK_8,             AKEYCODE_8);
        MAP(SDLK_9,             AKEYCODE_9);
        MAP(SDLK_SEMICOLON,     AKEYCODE_SEMICOLON);
        MAP(SDLK_EQUALS,        AKEYCODE_EQUALS);
        MAP(SDLK_AT,            AKEYCODE_AT);
        MAP(SDLK_LEFTBRACKET,   AKEYCODE_LEFT_BRACKET);
        MAP(SDLK_BACKSLASH,     AKEYCODE_BACKSLASH);
        MAP(SDLK_RIGHTBRACKET,  AKEYCODE_RIGHT_BRACKET);
        MAP(SDLK_BACKQUOTE,     AKEYCODE_GRAVE);
        MAP(SDLK_a,             AKEYCODE_A);
        MAP(SDLK_b,             AKEYCODE_B);
        MAP(SDLK_c,             AKEYCODE_C);
        MAP(SDLK_d,             AKEYCODE_D);
        MAP(SDLK_e,             AKEYCODE_E);
        MAP(SDLK_f,             AKEYCODE_F);
        MAP(SDLK_g,             AKEYCODE_G);
        MAP(SDLK_h,             AKEYCODE_H);
        MAP(SDLK_i,             AKEYCODE_I);
        MAP(SDLK_j,             AKEYCODE_J);
        MAP(SDLK_k,             AKEYCODE_K);
        MAP(SDLK_l,             AKEYCODE_L);
        MAP(SDLK_m,             AKEYCODE_M);
        MAP(SDLK_n,             AKEYCODE_N);
        MAP(SDLK_o,             AKEYCODE_O);
        MAP(SDLK_p,             AKEYCODE_P);
        MAP(SDLK_q,             AKEYCODE_Q);
        MAP(SDLK_r,             AKEYCODE_R);
        MAP(SDLK_s,             AKEYCODE_S);
        MAP(SDLK_t,             AKEYCODE_T);
        MAP(SDLK_u,             AKEYCODE_U);
        MAP(SDLK_v,             AKEYCODE_V);
        MAP(SDLK_w,             AKEYCODE_W);
        MAP(SDLK_x,             AKEYCODE_X);
        MAP(SDLK_y,             AKEYCODE_Y);
        MAP(SDLK_z,             AKEYCODE_Z);
        MAP(SDLK_KP_ENTER,      AKEYCODE_NUMPAD_ENTER);
        MAP(SDLK_KP_1,          AKEYCODE_NUMPAD_1);
        MAP(SDLK_KP_2,          AKEYCODE_NUMPAD_2);
        MAP(SDLK_KP_3,          AKEYCODE_NUMPAD_3);
        MAP(SDLK_KP_4,          AKEYCODE_NUMPAD_4);
        MAP(SDLK_KP_5,          AKEYCODE_NUMPAD_5);
        MAP(SDLK_KP_6,          AKEYCODE_NUMPAD_6);
        MAP(SDLK_KP_7,          AKEYCODE_NUMPAD_7);
        MAP(SDLK_KP_8,          AKEYCODE_NUMPAD_8);
        MAP(SDLK_KP_9,          AKEYCODE_NUMPAD_9);
        MAP(SDLK_KP_0,          AKEYCODE_NUMPAD_0);
        MAP(SDLK_KP_DIVIDE,     AKEYCODE_NUMPAD_DIVIDE);
        MAP(SDLK_KP_MULTIPLY,   AKEYCODE_NUMPAD_MULTIPLY);
        MAP(SDLK_KP_MINUS,      AKEYCODE_NUMPAD_SUBTRACT);
        MAP(SDLK_KP_PLUS,       AKEYCODE_NUMPAD_ADD);
        MAP(SDLK_KP_PERIOD,     AKEYCODE_NUMPAD_DOT);
        MAP(SDLK_KP_EQUALS,     AKEYCODE_NUMPAD_EQUALS);
        MAP(SDLK_KP_LEFTPAREN,  AKEYCODE_NUMPAD_LEFT_PAREN);
        MAP(SDLK_KP_RIGHTPAREN, AKEYCODE_NUMPAD_RIGHT_PAREN);
        FAIL;
    }
}

static SDL_bool convert_keycode(SDL_Keycode from, enum android_keycode *to) {
    switch (from) {
        MAP(SDLK_RETURN,       AKEYCODE_ENTER);
        MAP(SDLK_KP_ENTER,     AKEYCODE_NUMPAD_ENTER);
        MAP(SDLK_ESCAPE,       AKEYCODE_ESCAPE);
        MAP(SDLK_BACKSPACE,    AKEYCODE_DEL);
        MAP(SDLK_TAB,          AKEYCODE_TAB);
        MAP(SDLK_HOME,         AKEYCODE_HOME);
        MAP(SDLK_PAGEUP,       AKEYCODE_PAGE_UP);
        MAP(SDLK_DELETE,       AKEYCODE_FORWARD_DEL);
        MAP(SDLK_END,          AKEYCODE_MOVE_END);
        MAP(SDLK_PAGEDOWN,     AKEYCODE_PAGE_DOWN);
        MAP(SDLK_RIGHT,        AKEYCODE_DPAD_RIGHT);
        MAP(SDLK_LEFT,         AKEYCODE_DPAD_LEFT);
        MAP(SDLK_DOWN,         AKEYCODE_DPAD_DOWN);
        MAP(SDLK_UP,           AKEYCODE_DPAD_UP);
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

SDL_bool input_key_from_sdl_to_android(const SDL_KeyboardEvent *from,
                                       struct control_event *to,
                                       SDL_bool raw_key_events) {
    to->type = CONTROL_EVENT_TYPE_KEYCODE;

    if (!convert_keycode_action(from->type, &to->keycode_event.action)) {
        return SDL_FALSE;
    }

    if (!convert_keycode(from->keysym.sym, &to->keycode_event.keycode)) {
        if (!raw_key_events ||
                !convert_text_keycode(from->keysym.sym, &to->keycode_event.keycode)) {
            return SDL_FALSE;
        }
    }

    to->keycode_event.metastate = convert_meta_state(from->keysym.mod);

    return SDL_TRUE;
}

SDL_bool mouse_button_from_sdl_to_android(const SDL_MouseButtonEvent *from,
                                          struct size screen_size,
                                          struct control_event *to) {
    to->type = CONTROL_EVENT_TYPE_MOUSE;

    if (!convert_mouse_action(from->type, &to->mouse_event.action)) {
        return SDL_FALSE;
    }

    to->mouse_event.buttons = convert_mouse_buttons(SDL_BUTTON(from->button));
    to->mouse_event.position.screen_size = screen_size;
    to->mouse_event.position.point.x = from->x;
    to->mouse_event.position.point.y = from->y;

    return SDL_TRUE;
}

SDL_bool mouse_motion_from_sdl_to_android(const SDL_MouseMotionEvent *from,
                                          struct size screen_size,
                                          struct control_event *to) {
    to->type = CONTROL_EVENT_TYPE_MOUSE;
    to->mouse_event.action = AMOTION_EVENT_ACTION_MOVE;
    to->mouse_event.buttons = convert_mouse_buttons(from->state);
    to->mouse_event.position.screen_size = screen_size;
    to->mouse_event.position.point.x = from->x;
    to->mouse_event.position.point.y = from->y;

    return SDL_TRUE;
}

SDL_bool mouse_wheel_from_sdl_to_android(const SDL_MouseWheelEvent *from,
                                         struct position position,
                                         struct control_event *to) {
    to->type = CONTROL_EVENT_TYPE_SCROLL;

    to->scroll_event.position = position;

    int mul = from->direction == SDL_MOUSEWHEEL_NORMAL ? 1 : -1;
    // SDL behavior seems inconsistent between horizontal and vertical scrolling
    // so reverse the horizontal
    // <https://wiki.libsdl.org/SDL_MouseWheelEvent#Remarks>
    to->scroll_event.hscroll = -mul * from->x;
    to->scroll_event.vscroll = mul * from->y;

    return SDL_TRUE;
}
