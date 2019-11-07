#include "event_converter.h"

#include "config.h"

#define MAP(FROM, TO) case FROM: *to = TO; return true
#define FAIL default: return false

bool
convert_keycode_action(SDL_EventType from, enum android_keyevent_action *to) {
    switch (from) {
        MAP(SDL_KEYDOWN, AKEY_EVENT_ACTION_DOWN);
        MAP(SDL_KEYUP,   AKEY_EVENT_ACTION_UP);
        FAIL;
    }
}

static enum android_metastate
autocomplete_metastate(enum android_metastate metastate) {
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

enum android_metastate
convert_meta_state(SDL_Keymod mod) {
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

bool
convert_keycode(SDL_Keycode from, enum android_keycode *to, uint16_t mod,
                bool prefer_text) {
    switch (from) {
        MAP(SDLK_RETURN,       AKEYCODE_ENTER);
        MAP(SDLK_KP_ENTER,     AKEYCODE_NUMPAD_ENTER);
        MAP(SDLK_ESCAPE,       AKEYCODE_ESCAPE);
        MAP(SDLK_BACKSPACE,    AKEYCODE_DEL);
        MAP(SDLK_TAB,          AKEYCODE_TAB);
        MAP(SDLK_PAGEUP,       AKEYCODE_PAGE_UP);
        MAP(SDLK_DELETE,       AKEYCODE_FORWARD_DEL);
        MAP(SDLK_HOME,         AKEYCODE_MOVE_HOME);
        MAP(SDLK_END,          AKEYCODE_MOVE_END);
        MAP(SDLK_PAGEDOWN,     AKEYCODE_PAGE_DOWN);
        MAP(SDLK_RIGHT,        AKEYCODE_DPAD_RIGHT);
        MAP(SDLK_LEFT,         AKEYCODE_DPAD_LEFT);
        MAP(SDLK_DOWN,         AKEYCODE_DPAD_DOWN);
        MAP(SDLK_UP,           AKEYCODE_DPAD_UP);
    }

    if (prefer_text) {
        // do not forward alpha and space key events
        return false;
    }

    if (mod & (KMOD_LALT | KMOD_RALT | KMOD_LGUI | KMOD_RGUI)) {
        return false;
    }
    // if ALT and META are not pressed, also handle letters and space
    switch (from) {
        MAP(SDLK_a,            AKEYCODE_A);
        MAP(SDLK_b,            AKEYCODE_B);
        MAP(SDLK_c,            AKEYCODE_C);
        MAP(SDLK_d,            AKEYCODE_D);
        MAP(SDLK_e,            AKEYCODE_E);
        MAP(SDLK_f,            AKEYCODE_F);
        MAP(SDLK_g,            AKEYCODE_G);
        MAP(SDLK_h,            AKEYCODE_H);
        MAP(SDLK_i,            AKEYCODE_I);
        MAP(SDLK_j,            AKEYCODE_J);
        MAP(SDLK_k,            AKEYCODE_K);
        MAP(SDLK_l,            AKEYCODE_L);
        MAP(SDLK_m,            AKEYCODE_M);
        MAP(SDLK_n,            AKEYCODE_N);
        MAP(SDLK_o,            AKEYCODE_O);
        MAP(SDLK_p,            AKEYCODE_P);
        MAP(SDLK_q,            AKEYCODE_Q);
        MAP(SDLK_r,            AKEYCODE_R);
        MAP(SDLK_s,            AKEYCODE_S);
        MAP(SDLK_t,            AKEYCODE_T);
        MAP(SDLK_u,            AKEYCODE_U);
        MAP(SDLK_v,            AKEYCODE_V);
        MAP(SDLK_w,            AKEYCODE_W);
        MAP(SDLK_x,            AKEYCODE_X);
        MAP(SDLK_y,            AKEYCODE_Y);
        MAP(SDLK_z,            AKEYCODE_Z);
        MAP(SDLK_SPACE,        AKEYCODE_SPACE);
        FAIL;
    }
}

enum android_motionevent_buttons
convert_mouse_buttons(uint32_t state) {
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
    if (state & SDL_BUTTON_X1MASK) {
        buttons |= AMOTION_EVENT_BUTTON_BACK;
    }
    if (state & SDL_BUTTON_X2MASK) {
        buttons |= AMOTION_EVENT_BUTTON_FORWARD;
    }
    return buttons;
}

bool
convert_mouse_action(SDL_EventType from, enum android_motionevent_action *to) {
    switch (from) {
        MAP(SDL_MOUSEBUTTONDOWN, AMOTION_EVENT_ACTION_DOWN);
        MAP(SDL_MOUSEBUTTONUP,   AMOTION_EVENT_ACTION_UP);
        FAIL;
    }
}

bool
convert_touch_action(SDL_EventType from, enum android_motionevent_action *to) {
    switch (from) {
        MAP(SDL_FINGERMOTION, AMOTION_EVENT_ACTION_MOVE);
        MAP(SDL_FINGERDOWN,   AMOTION_EVENT_ACTION_DOWN);
        MAP(SDL_FINGERUP,     AMOTION_EVENT_ACTION_UP);
        FAIL;
    }
}
