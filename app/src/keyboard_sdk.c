#include "keyboard_sdk.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "android/input.h"
#include "android/keycodes.h"
#include "control_msg.h"
#include "controller.h"
#include "input_events.h"
#include "util/intmap.h"
#include "util/log.h"

/** Downcast key processor to sc_keyboard_sdk */
#define DOWNCAST(KP) container_of(KP, struct sc_keyboard_sdk, key_processor)

static enum android_keyevent_action
convert_keycode_action(enum sc_action action) {
    if (action == SC_ACTION_DOWN) {
        return AKEY_EVENT_ACTION_DOWN;
    }
    assert(action == SC_ACTION_UP);
    return AKEY_EVENT_ACTION_UP;
}

static bool
convert_keycode(enum sc_keycode from, enum android_keycode *to, uint16_t mod,
                enum sc_key_inject_mode key_inject_mode) {
    // Navigation keys and ENTER.
    // Used in all modes.
    static const struct sc_intmap_entry special_keys[] = {
        {SC_KEYCODE_RETURN,    AKEYCODE_ENTER},
        {SC_KEYCODE_KP_ENTER,  AKEYCODE_NUMPAD_ENTER},
        {SC_KEYCODE_ESCAPE,    AKEYCODE_ESCAPE},
        {SC_KEYCODE_BACKSPACE, AKEYCODE_DEL},
        {SC_KEYCODE_TAB,       AKEYCODE_TAB},
        {SC_KEYCODE_PAGEUP,    AKEYCODE_PAGE_UP},
        {SC_KEYCODE_DELETE,    AKEYCODE_FORWARD_DEL},
        {SC_KEYCODE_HOME,      AKEYCODE_MOVE_HOME},
        {SC_KEYCODE_END,       AKEYCODE_MOVE_END},
        {SC_KEYCODE_PAGEDOWN,  AKEYCODE_PAGE_DOWN},
        {SC_KEYCODE_RIGHT,     AKEYCODE_DPAD_RIGHT},
        {SC_KEYCODE_LEFT,      AKEYCODE_DPAD_LEFT},
        {SC_KEYCODE_DOWN,      AKEYCODE_DPAD_DOWN},
        {SC_KEYCODE_UP,        AKEYCODE_DPAD_UP},
        {SC_KEYCODE_LCTRL,     AKEYCODE_CTRL_LEFT},
        {SC_KEYCODE_RCTRL,     AKEYCODE_CTRL_RIGHT},
        {SC_KEYCODE_LSHIFT,    AKEYCODE_SHIFT_LEFT},
        {SC_KEYCODE_RSHIFT,    AKEYCODE_SHIFT_RIGHT},
        {SC_KEYCODE_LALT,      AKEYCODE_ALT_LEFT},
        {SC_KEYCODE_RALT,      AKEYCODE_ALT_RIGHT},
        {SC_KEYCODE_LGUI,      AKEYCODE_META_LEFT},
        {SC_KEYCODE_RGUI,      AKEYCODE_META_RIGHT},
    };

    // Numpad navigation keys.
    // Used in all modes, when NumLock and Shift are disabled.
    static const struct sc_intmap_entry kp_nav_keys[] = {
        {SC_KEYCODE_KP_0,      AKEYCODE_INSERT},
        {SC_KEYCODE_KP_1,      AKEYCODE_MOVE_END},
        {SC_KEYCODE_KP_2,      AKEYCODE_DPAD_DOWN},
        {SC_KEYCODE_KP_3,      AKEYCODE_PAGE_DOWN},
        {SC_KEYCODE_KP_4,      AKEYCODE_DPAD_LEFT},
        {SC_KEYCODE_KP_6,      AKEYCODE_DPAD_RIGHT},
        {SC_KEYCODE_KP_7,      AKEYCODE_MOVE_HOME},
        {SC_KEYCODE_KP_8,      AKEYCODE_DPAD_UP},
        {SC_KEYCODE_KP_9,      AKEYCODE_PAGE_UP},
        {SC_KEYCODE_KP_PERIOD, AKEYCODE_FORWARD_DEL},
    };

    // Letters and space.
    // Used in non-text mode.
    static const struct sc_intmap_entry alphaspace_keys[] = {
        {SC_KEYCODE_a,         AKEYCODE_A},
        {SC_KEYCODE_b,         AKEYCODE_B},
        {SC_KEYCODE_c,         AKEYCODE_C},
        {SC_KEYCODE_d,         AKEYCODE_D},
        {SC_KEYCODE_e,         AKEYCODE_E},
        {SC_KEYCODE_f,         AKEYCODE_F},
        {SC_KEYCODE_g,         AKEYCODE_G},
        {SC_KEYCODE_h,         AKEYCODE_H},
        {SC_KEYCODE_i,         AKEYCODE_I},
        {SC_KEYCODE_j,         AKEYCODE_J},
        {SC_KEYCODE_k,         AKEYCODE_K},
        {SC_KEYCODE_l,         AKEYCODE_L},
        {SC_KEYCODE_m,         AKEYCODE_M},
        {SC_KEYCODE_n,         AKEYCODE_N},
        {SC_KEYCODE_o,         AKEYCODE_O},
        {SC_KEYCODE_p,         AKEYCODE_P},
        {SC_KEYCODE_q,         AKEYCODE_Q},
        {SC_KEYCODE_r,         AKEYCODE_R},
        {SC_KEYCODE_s,         AKEYCODE_S},
        {SC_KEYCODE_t,         AKEYCODE_T},
        {SC_KEYCODE_u,         AKEYCODE_U},
        {SC_KEYCODE_v,         AKEYCODE_V},
        {SC_KEYCODE_w,         AKEYCODE_W},
        {SC_KEYCODE_x,         AKEYCODE_X},
        {SC_KEYCODE_y,         AKEYCODE_Y},
        {SC_KEYCODE_z,         AKEYCODE_Z},
        {SC_KEYCODE_SPACE,     AKEYCODE_SPACE},
    };

    // Numbers and punctuation keys.
    // Used in raw mode only.
    static const struct sc_intmap_entry numbers_punct_keys[] = {
        {SC_KEYCODE_HASH,          AKEYCODE_POUND},
        {SC_KEYCODE_PERCENT,       AKEYCODE_PERIOD},
        {SC_KEYCODE_QUOTE,         AKEYCODE_APOSTROPHE},
        {SC_KEYCODE_ASTERISK,      AKEYCODE_STAR},
        {SC_KEYCODE_PLUS,          AKEYCODE_PLUS},
        {SC_KEYCODE_COMMA,         AKEYCODE_COMMA},
        {SC_KEYCODE_MINUS,         AKEYCODE_MINUS},
        {SC_KEYCODE_PERIOD,        AKEYCODE_PERIOD},
        {SC_KEYCODE_SLASH,         AKEYCODE_SLASH},
        {SC_KEYCODE_0,             AKEYCODE_0},
        {SC_KEYCODE_1,             AKEYCODE_1},
        {SC_KEYCODE_2,             AKEYCODE_2},
        {SC_KEYCODE_3,             AKEYCODE_3},
        {SC_KEYCODE_4,             AKEYCODE_4},
        {SC_KEYCODE_5,             AKEYCODE_5},
        {SC_KEYCODE_6,             AKEYCODE_6},
        {SC_KEYCODE_7,             AKEYCODE_7},
        {SC_KEYCODE_8,             AKEYCODE_8},
        {SC_KEYCODE_9,             AKEYCODE_9},
        {SC_KEYCODE_SEMICOLON,     AKEYCODE_SEMICOLON},
        {SC_KEYCODE_EQUALS,        AKEYCODE_EQUALS},
        {SC_KEYCODE_AT,            AKEYCODE_AT},
        {SC_KEYCODE_LEFTBRACKET,   AKEYCODE_LEFT_BRACKET},
        {SC_KEYCODE_BACKSLASH,     AKEYCODE_BACKSLASH},
        {SC_KEYCODE_RIGHTBRACKET,  AKEYCODE_RIGHT_BRACKET},
        {SC_KEYCODE_BACKQUOTE,     AKEYCODE_GRAVE},
        {SC_KEYCODE_KP_1,          AKEYCODE_NUMPAD_1},
        {SC_KEYCODE_KP_2,          AKEYCODE_NUMPAD_2},
        {SC_KEYCODE_KP_3,          AKEYCODE_NUMPAD_3},
        {SC_KEYCODE_KP_4,          AKEYCODE_NUMPAD_4},
        {SC_KEYCODE_KP_5,          AKEYCODE_NUMPAD_5},
        {SC_KEYCODE_KP_6,          AKEYCODE_NUMPAD_6},
        {SC_KEYCODE_KP_7,          AKEYCODE_NUMPAD_7},
        {SC_KEYCODE_KP_8,          AKEYCODE_NUMPAD_8},
        {SC_KEYCODE_KP_9,          AKEYCODE_NUMPAD_9},
        {SC_KEYCODE_KP_0,          AKEYCODE_NUMPAD_0},
        {SC_KEYCODE_KP_DIVIDE,     AKEYCODE_NUMPAD_DIVIDE},
        {SC_KEYCODE_KP_MULTIPLY,   AKEYCODE_NUMPAD_MULTIPLY},
        {SC_KEYCODE_KP_MINUS,      AKEYCODE_NUMPAD_SUBTRACT},
        {SC_KEYCODE_KP_PLUS,       AKEYCODE_NUMPAD_ADD},
        {SC_KEYCODE_KP_PERIOD,     AKEYCODE_NUMPAD_DOT},
        {SC_KEYCODE_KP_EQUALS,     AKEYCODE_NUMPAD_EQUALS},
        {SC_KEYCODE_KP_LEFTPAREN,  AKEYCODE_NUMPAD_LEFT_PAREN},
        {SC_KEYCODE_KP_RIGHTPAREN, AKEYCODE_NUMPAD_RIGHT_PAREN},
    };

    const struct sc_intmap_entry *entry =
        SC_INTMAP_FIND_ENTRY(special_keys, from);
    if (entry) {
        *to = entry->value;
        return true;
    }

    if (!(mod & (SC_MOD_NUM | SC_MOD_LSHIFT | SC_MOD_RSHIFT))) {
        // Handle Numpad events when Num Lock is disabled
        // If SHIFT is pressed, a text event will be sent instead
        entry = SC_INTMAP_FIND_ENTRY(kp_nav_keys, from);
        if (entry) {
            *to = entry->value;
            return true;
        }
    }

    if (key_inject_mode == SC_KEY_INJECT_MODE_TEXT &&
            !(mod & (SC_MOD_LCTRL | SC_MOD_RCTRL))) {
        // do not forward alpha and space key events (unless Ctrl is pressed)
        return false;
    }

    // Handle letters and space
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
convert_meta_state(uint16_t mod) {
    enum android_metastate metastate = 0;
    if (mod & SC_MOD_LSHIFT) {
        metastate |= AMETA_SHIFT_LEFT_ON;
    }
    if (mod & SC_MOD_RSHIFT) {
        metastate |= AMETA_SHIFT_RIGHT_ON;
    }
    if (mod & SC_MOD_LCTRL) {
        metastate |= AMETA_CTRL_LEFT_ON;
    }
    if (mod & SC_MOD_RCTRL) {
        metastate |= AMETA_CTRL_RIGHT_ON;
    }
    if (mod & SC_MOD_LALT) {
        metastate |= AMETA_ALT_LEFT_ON;
    }
    if (mod & SC_MOD_RALT) {
        metastate |= AMETA_ALT_RIGHT_ON;
    }
    if (mod & SC_MOD_LGUI) { // Windows key
        metastate |= AMETA_META_LEFT_ON;
    }
    if (mod & SC_MOD_RGUI) { // Windows key
        metastate |= AMETA_META_RIGHT_ON;
    }
    if (mod & SC_MOD_NUM) {
        metastate |= AMETA_NUM_LOCK_ON;
    }
    if (mod & SC_MOD_CAPS) {
        metastate |= AMETA_CAPS_LOCK_ON;
    }

    // fill the dependent fields
    return autocomplete_metastate(metastate);
}

static bool
convert_input_key(const struct sc_key_event *event, struct sc_control_msg *msg,
                  enum sc_key_inject_mode key_inject_mode, uint32_t repeat) {
    msg->type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE;

    if (!convert_keycode(event->keycode, &msg->inject_keycode.keycode,
                         event->mods_state, key_inject_mode)) {
        return false;
    }

    msg->inject_keycode.action = convert_keycode_action(event->action);
    msg->inject_keycode.repeat = repeat;
    msg->inject_keycode.metastate = convert_meta_state(event->mods_state);

    return true;
}

static void
sc_key_processor_process_key(struct sc_key_processor *kp,
                             const struct sc_key_event *event,
                             uint64_t ack_to_wait) {
    // The device clipboard synchronization and the key event messages are
    // serialized, there is nothing special to do to ensure that the clipboard
    // is set before injecting Ctrl+v.
    (void) ack_to_wait;

    struct sc_keyboard_sdk *kb = DOWNCAST(kp);

    if (event->repeat) {
        if (!kb->forward_key_repeat) {
            return;
        }
        ++kb->repeat;
    } else {
        kb->repeat = 0;
    }

    struct sc_control_msg msg;
    if (convert_input_key(event, &msg, kb->key_inject_mode, kb->repeat)) {
        if (!sc_controller_push_msg(kb->controller, &msg)) {
            LOGW("Could not request 'inject keycode'");
        }
    }
}

static void
sc_key_processor_process_text(struct sc_key_processor *kp,
                              const struct sc_text_event *event) {
    struct sc_keyboard_sdk *kb = DOWNCAST(kp);

    if (kb->key_inject_mode == SC_KEY_INJECT_MODE_RAW) {
        // Never inject text events
        return;
    }

    if (kb->key_inject_mode == SC_KEY_INJECT_MODE_MIXED) {
        char c = event->text[0];
        if (isalpha(c) || c == ' ') {
            assert(event->text[1] == '\0');
            // Letters and space are handled as raw key events
            return;
        }
    }

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = strdup(event->text);
    if (!msg.inject_text.text) {
        LOGW("Could not strdup input text");
        return;
    }
    if (!sc_controller_push_msg(kb->controller, &msg)) {
        free(msg.inject_text.text);
        LOGW("Could not request 'inject text'");
    }
}

void
sc_keyboard_sdk_init(struct sc_keyboard_sdk *kb,
                     struct sc_controller *controller,
                     enum sc_key_inject_mode key_inject_mode,
                     bool forward_key_repeat) {
    kb->controller = controller;
    kb->key_inject_mode = key_inject_mode;
    kb->forward_key_repeat = forward_key_repeat;

    kb->repeat = 0;

    static const struct sc_key_processor_ops ops = {
        .process_key = sc_key_processor_process_key,
        .process_text = sc_key_processor_process_text,
    };

    // Key injection and clipboard synchronization are serialized
    kb->key_processor.async_paste = false;
    kb->key_processor.hid = false;
    kb->key_processor.ops = &ops;
}
