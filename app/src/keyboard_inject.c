#include "keyboard_inject.h"

#include <assert.h>
#include <SDL2/SDL_events.h>

#include "android/input.h"
#include "control_msg.h"
#include "controller.h"
#include "util/log.h"

/** Downcast key processor to sc_keyboard_inject */
#define DOWNCAST(KP) \
    container_of(KP, struct sc_keyboard_inject, key_processor)

#define MAP(FROM, TO) case FROM: *to = TO; return true
#define FAIL default: return false
static bool
convert_keycode_action(SDL_EventType from, enum android_keyevent_action *to) {
    switch (from) {
        MAP(SDL_KEYDOWN, AKEY_EVENT_ACTION_DOWN);
        MAP(SDL_KEYUP,   AKEY_EVENT_ACTION_UP);
        FAIL;
    }
}

static bool
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
        MAP(SDLK_LCTRL,        AKEYCODE_CTRL_LEFT);
        MAP(SDLK_RCTRL,        AKEYCODE_CTRL_RIGHT);
        MAP(SDLK_LSHIFT,       AKEYCODE_SHIFT_LEFT);
        MAP(SDLK_RSHIFT,       AKEYCODE_SHIFT_RIGHT);
    }

    if (!(mod & (KMOD_NUM | KMOD_SHIFT))) {
        // Handle Numpad events when Num Lock is disabled
        // If SHIFT is pressed, a text event will be sent instead
        switch(from) {
            MAP(SDLK_KP_0,            AKEYCODE_INSERT);
            MAP(SDLK_KP_1,            AKEYCODE_MOVE_END);
            MAP(SDLK_KP_2,            AKEYCODE_DPAD_DOWN);
            MAP(SDLK_KP_3,            AKEYCODE_PAGE_DOWN);
            MAP(SDLK_KP_4,            AKEYCODE_DPAD_LEFT);
            MAP(SDLK_KP_6,            AKEYCODE_DPAD_RIGHT);
            MAP(SDLK_KP_7,            AKEYCODE_MOVE_HOME);
            MAP(SDLK_KP_8,            AKEYCODE_DPAD_UP);
            MAP(SDLK_KP_9,            AKEYCODE_PAGE_UP);
            MAP(SDLK_KP_PERIOD,       AKEYCODE_FORWARD_DEL);
        }
    }

    if (prefer_text && !(mod & KMOD_CTRL)) {
        // do not forward alpha and space key events (unless Ctrl is pressed)
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

static enum android_metastate
autocomplete_metastate(enum android_metastate metastate) {
    // fill dependent flags
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

static enum android_metastate
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

static bool
convert_input_key(const SDL_KeyboardEvent *from, struct control_msg *to,
                  bool prefer_text, uint32_t repeat) {
    to->type = CONTROL_MSG_TYPE_INJECT_KEYCODE;

    if (!convert_keycode_action(from->type, &to->inject_keycode.action)) {
        return false;
    }

    uint16_t mod = from->keysym.mod;
    if (!convert_keycode(from->keysym.sym, &to->inject_keycode.keycode, mod,
                         prefer_text)) {
        return false;
    }

    to->inject_keycode.repeat = repeat;
    to->inject_keycode.metastate = convert_meta_state(mod);

    return true;
}

static void
sc_key_processor_process_key(struct sc_key_processor *kp,
                             const SDL_KeyboardEvent *event) {
    struct sc_keyboard_inject *ki = DOWNCAST(kp);

    if (event->repeat) {
        if (!ki->forward_key_repeat) {
            return;
        }
        ++ki->repeat;
    } else {
        ki->repeat = 0;
    }

    struct control_msg msg;
    if (convert_input_key(event, &msg, ki->prefer_text, ki->repeat)) {
        if (!controller_push_msg(ki->controller, &msg)) {
            LOGW("Could not request 'inject keycode'");
        }
    }
}

static void
sc_key_processor_process_text(struct sc_key_processor *kp,
                              const SDL_TextInputEvent *event) {
    struct sc_keyboard_inject *ki = DOWNCAST(kp);

    if (!ki->prefer_text) {
        char c = event->text[0];
        if (isalpha(c) || c == ' ') {
            assert(event->text[1] == '\0');
            // letters and space are handled as raw key event
            return;
        }
    }

    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = strdup(event->text);
    if (!msg.inject_text.text) {
        LOGW("Could not strdup input text");
        return;
    }
    if (!controller_push_msg(ki->controller, &msg)) {
        free(msg.inject_text.text);
        LOGW("Could not request 'inject text'");
    }
}

void
sc_keyboard_inject_init(struct sc_keyboard_inject *ki,
                        struct controller *controller,
                        const struct scrcpy_options *options) {
    ki->controller = controller;
    ki->prefer_text = options->prefer_text;
    ki->forward_key_repeat = options->forward_key_repeat;

    ki->repeat = 0;

    static const struct sc_key_processor_ops ops = {
        .process_key = sc_key_processor_process_key,
        .process_text = sc_key_processor_process_text,
    };

    ki->key_processor.ops = &ops;
}
