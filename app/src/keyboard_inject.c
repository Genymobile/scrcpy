#include "keyboard_inject.h"

#include <assert.h>
#include <SDL2/SDL_events.h>

#include "android/input.h"
#include "control_msg.h"
#include "controller.h"
#include "util/intmap.h"
#include "util/log.h"

/** Downcast key processor to sc_keyboard_inject */
#define DOWNCAST(KP) container_of(KP, struct sc_keyboard_inject, key_processor)

static bool
convert_keycode_action(SDL_EventType from, enum android_keyevent_action *to) {
    static const struct sc_intmap_entry actions[] = {
        {SDL_KEYDOWN, AKEY_EVENT_ACTION_DOWN},
        {SDL_KEYUP,   AKEY_EVENT_ACTION_UP},
    };

    const struct sc_intmap_entry *entry = SC_INTMAP_FIND_ENTRY(actions, from);
    if (entry) {
        *to = entry->value;
        return true;
    }

    return false;
}

static bool
convert_keycode(SDL_Keycode from, enum android_keycode *to, uint16_t mod,
                enum sc_key_inject_mode key_inject_mode) {
    // Navigation keys and ENTER.
    // Used in all modes.
    static const struct sc_intmap_entry special_keys[] = {
        {SDLK_RETURN,    AKEYCODE_ENTER},
        {SDLK_KP_ENTER,  AKEYCODE_NUMPAD_ENTER},
        {SDLK_ESCAPE,    AKEYCODE_ESCAPE},
        {SDLK_BACKSPACE, AKEYCODE_DEL},
        {SDLK_TAB,       AKEYCODE_TAB},
        {SDLK_PAGEUP,    AKEYCODE_PAGE_UP},
        {SDLK_DELETE,    AKEYCODE_FORWARD_DEL},
        {SDLK_HOME,      AKEYCODE_MOVE_HOME},
        {SDLK_END,       AKEYCODE_MOVE_END},
        {SDLK_PAGEDOWN,  AKEYCODE_PAGE_DOWN},
        {SDLK_RIGHT,     AKEYCODE_DPAD_RIGHT},
        {SDLK_LEFT,      AKEYCODE_DPAD_LEFT},
        {SDLK_DOWN,      AKEYCODE_DPAD_DOWN},
        {SDLK_UP,        AKEYCODE_DPAD_UP},
        {SDLK_LCTRL,     AKEYCODE_CTRL_LEFT},
        {SDLK_RCTRL,     AKEYCODE_CTRL_RIGHT},
        {SDLK_LSHIFT,    AKEYCODE_SHIFT_LEFT},
        {SDLK_RSHIFT,    AKEYCODE_SHIFT_RIGHT},
    };

    // Numpad navigation keys.
    // Used in all modes, when NumLock and Shift are disabled.
    static const struct sc_intmap_entry kp_nav_keys[] = {
        {SDLK_KP_0,      AKEYCODE_INSERT},
        {SDLK_KP_1,      AKEYCODE_MOVE_END},
        {SDLK_KP_2,      AKEYCODE_DPAD_DOWN},
        {SDLK_KP_3,      AKEYCODE_PAGE_DOWN},
        {SDLK_KP_4,      AKEYCODE_DPAD_LEFT},
        {SDLK_KP_6,      AKEYCODE_DPAD_RIGHT},
        {SDLK_KP_7,      AKEYCODE_MOVE_HOME},
        {SDLK_KP_8,      AKEYCODE_DPAD_UP},
        {SDLK_KP_9,      AKEYCODE_PAGE_UP},
        {SDLK_KP_PERIOD, AKEYCODE_FORWARD_DEL},
    };

    // Letters and space.
    // Used in non-text mode.
    static const struct sc_intmap_entry alphaspace_keys[] = {
        {SDLK_a,         AKEYCODE_A},
        {SDLK_b,         AKEYCODE_B},
        {SDLK_c,         AKEYCODE_C},
        {SDLK_d,         AKEYCODE_D},
        {SDLK_e,         AKEYCODE_E},
        {SDLK_f,         AKEYCODE_F},
        {SDLK_g,         AKEYCODE_G},
        {SDLK_h,         AKEYCODE_H},
        {SDLK_i,         AKEYCODE_I},
        {SDLK_j,         AKEYCODE_J},
        {SDLK_k,         AKEYCODE_K},
        {SDLK_l,         AKEYCODE_L},
        {SDLK_m,         AKEYCODE_M},
        {SDLK_n,         AKEYCODE_N},
        {SDLK_o,         AKEYCODE_O},
        {SDLK_p,         AKEYCODE_P},
        {SDLK_q,         AKEYCODE_Q},
        {SDLK_r,         AKEYCODE_R},
        {SDLK_s,         AKEYCODE_S},
        {SDLK_t,         AKEYCODE_T},
        {SDLK_u,         AKEYCODE_U},
        {SDLK_v,         AKEYCODE_V},
        {SDLK_w,         AKEYCODE_W},
        {SDLK_x,         AKEYCODE_X},
        {SDLK_y,         AKEYCODE_Y},
        {SDLK_z,         AKEYCODE_Z},
        {SDLK_SPACE,     AKEYCODE_SPACE},
    };

    // Numbers and punctuation keys.
    // Used in raw mode only.
    static const struct sc_intmap_entry numbers_punct_keys[] = {
        {SDLK_HASH,          AKEYCODE_POUND},
        {SDLK_PERCENT,       AKEYCODE_PERIOD},
        {SDLK_QUOTE,         AKEYCODE_APOSTROPHE},
        {SDLK_ASTERISK,      AKEYCODE_STAR},
        {SDLK_PLUS,          AKEYCODE_PLUS},
        {SDLK_COMMA,         AKEYCODE_COMMA},
        {SDLK_MINUS,         AKEYCODE_MINUS},
        {SDLK_PERIOD,        AKEYCODE_PERIOD},
        {SDLK_SLASH,         AKEYCODE_SLASH},
        {SDLK_0,             AKEYCODE_0},
        {SDLK_1,             AKEYCODE_1},
        {SDLK_2,             AKEYCODE_2},
        {SDLK_3,             AKEYCODE_3},
        {SDLK_4,             AKEYCODE_4},
        {SDLK_5,             AKEYCODE_5},
        {SDLK_6,             AKEYCODE_6},
        {SDLK_7,             AKEYCODE_7},
        {SDLK_8,             AKEYCODE_8},
        {SDLK_9,             AKEYCODE_9},
        {SDLK_SEMICOLON,     AKEYCODE_SEMICOLON},
        {SDLK_EQUALS,        AKEYCODE_EQUALS},
        {SDLK_AT,            AKEYCODE_AT},
        {SDLK_LEFTBRACKET,   AKEYCODE_LEFT_BRACKET},
        {SDLK_BACKSLASH,     AKEYCODE_BACKSLASH},
        {SDLK_RIGHTBRACKET,  AKEYCODE_RIGHT_BRACKET},
        {SDLK_BACKQUOTE,     AKEYCODE_GRAVE},
        {SDLK_KP_1,          AKEYCODE_NUMPAD_1},
        {SDLK_KP_2,          AKEYCODE_NUMPAD_2},
        {SDLK_KP_3,          AKEYCODE_NUMPAD_3},
        {SDLK_KP_4,          AKEYCODE_NUMPAD_4},
        {SDLK_KP_5,          AKEYCODE_NUMPAD_5},
        {SDLK_KP_6,          AKEYCODE_NUMPAD_6},
        {SDLK_KP_7,          AKEYCODE_NUMPAD_7},
        {SDLK_KP_8,          AKEYCODE_NUMPAD_8},
        {SDLK_KP_9,          AKEYCODE_NUMPAD_9},
        {SDLK_KP_0,          AKEYCODE_NUMPAD_0},
        {SDLK_KP_DIVIDE,     AKEYCODE_NUMPAD_DIVIDE},
        {SDLK_KP_MULTIPLY,   AKEYCODE_NUMPAD_MULTIPLY},
        {SDLK_KP_MINUS,      AKEYCODE_NUMPAD_SUBTRACT},
        {SDLK_KP_PLUS,       AKEYCODE_NUMPAD_ADD},
        {SDLK_KP_PERIOD,     AKEYCODE_NUMPAD_DOT},
        {SDLK_KP_EQUALS,     AKEYCODE_NUMPAD_EQUALS},
        {SDLK_KP_LEFTPAREN,  AKEYCODE_NUMPAD_LEFT_PAREN},
        {SDLK_KP_RIGHTPAREN, AKEYCODE_NUMPAD_RIGHT_PAREN},
    };

    const struct sc_intmap_entry *entry =
        SC_INTMAP_FIND_ENTRY(special_keys, from);
    if (entry) {
        *to = entry->value;
        return true;
    }

    if (!(mod & (KMOD_NUM | KMOD_SHIFT))) {
        // Handle Numpad events when Num Lock is disabled
        // If SHIFT is pressed, a text event will be sent instead
        entry = SC_INTMAP_FIND_ENTRY(kp_nav_keys, from);
        if (entry) {
            *to = entry->value;
            return true;
        }
    }

    if (key_inject_mode == SC_KEY_INJECT_MODE_TEXT && !(mod & KMOD_CTRL)) {
        // do not forward alpha and space key events (unless Ctrl is pressed)
        return false;
    }

    if (mod & (KMOD_LALT | KMOD_RALT | KMOD_LGUI | KMOD_RGUI)) {
        return false;
    }

    // if ALT and META are not pressed, also handle letters and space
    entry = SC_INTMAP_FIND_ENTRY(alphaspace_keys, from);
    if (entry) {
        *to = entry->value;
        return true;
    }

    if (key_inject_mode == SC_KEY_INJECT_MODE_RAW) {
        entry = SC_INTMAP_FIND_ENTRY(numbers_punct_keys, from);
        if (entry) {
            *to = entry->value;
            return true;
        }
    }

    return false;
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
                  enum sc_key_inject_mode key_inject_mode, uint32_t repeat) {
    to->type = CONTROL_MSG_TYPE_INJECT_KEYCODE;

    if (!convert_keycode_action(from->type, &to->inject_keycode.action)) {
        return false;
    }

    uint16_t mod = from->keysym.mod;
    if (!convert_keycode(from->keysym.sym, &to->inject_keycode.keycode, mod,
                         key_inject_mode)) {
        return false;
    }

    to->inject_keycode.repeat = repeat;
    to->inject_keycode.metastate = convert_meta_state(mod);

    return true;
}

static void
sc_key_processor_process_key(struct sc_key_processor *kp,
                             const SDL_KeyboardEvent *event,
                             uint64_t ack_to_wait) {
    // The device clipboard synchronization and the key event messages are
    // serialized, there is nothing special to do to ensure that the clipboard
    // is set before injecting Ctrl+v.
    (void) ack_to_wait;

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
    if (convert_input_key(event, &msg, ki->key_inject_mode, ki->repeat)) {
        if (!controller_push_msg(ki->controller, &msg)) {
            LOGW("Could not request 'inject keycode'");
        }
    }
}

static void
sc_key_processor_process_text(struct sc_key_processor *kp,
                              const SDL_TextInputEvent *event) {
    struct sc_keyboard_inject *ki = DOWNCAST(kp);

    if (ki->key_inject_mode == SC_KEY_INJECT_MODE_RAW) {
        // Never inject text events
        return;
    }

    if (ki->key_inject_mode == SC_KEY_INJECT_MODE_MIXED) {
        char c = event->text[0];
        if (isalpha(c) || c == ' ') {
            assert(event->text[1] == '\0');
            // Letters and space are handled as raw key events
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
    ki->key_inject_mode = options->key_inject_mode;
    ki->forward_key_repeat = options->forward_key_repeat;

    ki->repeat = 0;

    static const struct sc_key_processor_ops ops = {
        .process_key = sc_key_processor_process_key,
        .process_text = sc_key_processor_process_text,
    };

    // Key injection and clipboard synchronization are serialized
    ki->key_processor.async_paste = false;
    ki->key_processor.ops = &ops;
}
