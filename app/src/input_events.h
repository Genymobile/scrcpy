#ifndef SC_INPUT_EVENTS_H
#define SC_INPUT_EVENTS_H

#include "common.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_events.h>

#include "coords.h"
#include "options.h"

/* The representation of input events in scrcpy is very close to the SDL API,
 * for simplicity.
 *
 * This scrcpy input events API is designed to be consumed by input event
 * processors (sc_key_processor and sc_mouse_processor, see app/src/trait/).
 *
 * One major semantic difference between SDL input events and scrcpy input
 * events is their frame of reference (for mouse and touch events): SDL events
 * coordinates are expressed in SDL window coordinates (the visible UI), while
 * scrcpy events are expressed in device frame coordinates.
 *
 * In particular, the window may be visually scaled or rotated (with --rotation
 * or MOD+Left/Right), but this does not impact scrcpy input events (contrary
 * to SDL input events). This allows to abstract these display details from the
 * input event processors (and to make them independent from the "screen").
 *
 * For many enums below, the values are purposely the same as the SDL
 * constants (though not all SDL values are represented), so that the
 * implementation to convert from the SDL version to the scrcpy version is
 * straightforward.
 *
 * In practice, there are 3 levels of input events:
 *  1. SDL input events (as received from SDL)
 *  2. scrcpy input events (this API)
 *  3. the key/mouse processors input events (Android API or HID events)
 *
 * An input event is first received (1), then (if accepted) converted to an
 * scrcpy input event (2), then submitted to the relevant key/mouse processor,
 * which (if accepted) is converted to an Android event (to be sent to the
 * server) or to an HID event (to be sent over USB/AOA directly).
 */

enum sc_mod {
    SC_MOD_LSHIFT = KMOD_LSHIFT,
    SC_MOD_RSHIFT = KMOD_RSHIFT,
    SC_MOD_LCTRL = KMOD_LCTRL,
    SC_MOD_RCTRL = KMOD_RCTRL,
    SC_MOD_LALT = KMOD_LALT,
    SC_MOD_RALT = KMOD_RALT,
    SC_MOD_LGUI = KMOD_LGUI,
    SC_MOD_RGUI = KMOD_RGUI,

    SC_MOD_NUM = KMOD_NUM,
    SC_MOD_CAPS = KMOD_CAPS,
};

enum sc_action {
    SC_ACTION_DOWN, // key or button pressed
    SC_ACTION_UP, // key or button released
};

enum sc_keycode {
    SC_KEYCODE_UNKNOWN = SDLK_UNKNOWN,

    SC_KEYCODE_RETURN = SDLK_RETURN,
    SC_KEYCODE_ESCAPE = SDLK_ESCAPE,
    SC_KEYCODE_BACKSPACE = SDLK_BACKSPACE,
    SC_KEYCODE_TAB = SDLK_TAB,
    SC_KEYCODE_SPACE = SDLK_SPACE,
    SC_KEYCODE_EXCLAIM = SDLK_EXCLAIM,
    SC_KEYCODE_QUOTEDBL = SDLK_QUOTEDBL,
    SC_KEYCODE_HASH = SDLK_HASH,
    SC_KEYCODE_PERCENT = SDLK_PERCENT,
    SC_KEYCODE_DOLLAR = SDLK_DOLLAR,
    SC_KEYCODE_AMPERSAND = SDLK_AMPERSAND,
    SC_KEYCODE_QUOTE = SDLK_QUOTE,
    SC_KEYCODE_LEFTPAREN = SDLK_LEFTPAREN,
    SC_KEYCODE_RIGHTPAREN = SDLK_RIGHTPAREN,
    SC_KEYCODE_ASTERISK = SDLK_ASTERISK,
    SC_KEYCODE_PLUS = SDLK_PLUS,
    SC_KEYCODE_COMMA = SDLK_COMMA,
    SC_KEYCODE_MINUS = SDLK_MINUS,
    SC_KEYCODE_PERIOD = SDLK_PERIOD,
    SC_KEYCODE_SLASH = SDLK_SLASH,
    SC_KEYCODE_0 = SDLK_0,
    SC_KEYCODE_1 = SDLK_1,
    SC_KEYCODE_2 = SDLK_2,
    SC_KEYCODE_3 = SDLK_3,
    SC_KEYCODE_4 = SDLK_4,
    SC_KEYCODE_5 = SDLK_5,
    SC_KEYCODE_6 = SDLK_6,
    SC_KEYCODE_7 = SDLK_7,
    SC_KEYCODE_8 = SDLK_8,
    SC_KEYCODE_9 = SDLK_9,
    SC_KEYCODE_COLON = SDLK_COLON,
    SC_KEYCODE_SEMICOLON = SDLK_SEMICOLON,
    SC_KEYCODE_LESS = SDLK_LESS,
    SC_KEYCODE_EQUALS = SDLK_EQUALS,
    SC_KEYCODE_GREATER = SDLK_GREATER,
    SC_KEYCODE_QUESTION = SDLK_QUESTION,
    SC_KEYCODE_AT = SDLK_AT,

    SC_KEYCODE_LEFTBRACKET = SDLK_LEFTBRACKET,
    SC_KEYCODE_BACKSLASH = SDLK_BACKSLASH,
    SC_KEYCODE_RIGHTBRACKET = SDLK_RIGHTBRACKET,
    SC_KEYCODE_CARET = SDLK_CARET,
    SC_KEYCODE_UNDERSCORE = SDLK_UNDERSCORE,
    SC_KEYCODE_BACKQUOTE = SDLK_BACKQUOTE,
    SC_KEYCODE_a = SDLK_a,
    SC_KEYCODE_b = SDLK_b,
    SC_KEYCODE_c = SDLK_c,
    SC_KEYCODE_d = SDLK_d,
    SC_KEYCODE_e = SDLK_e,
    SC_KEYCODE_f = SDLK_f,
    SC_KEYCODE_g = SDLK_g,
    SC_KEYCODE_h = SDLK_h,
    SC_KEYCODE_i = SDLK_i,
    SC_KEYCODE_j = SDLK_j,
    SC_KEYCODE_k = SDLK_k,
    SC_KEYCODE_l = SDLK_l,
    SC_KEYCODE_m = SDLK_m,
    SC_KEYCODE_n = SDLK_n,
    SC_KEYCODE_o = SDLK_o,
    SC_KEYCODE_p = SDLK_p,
    SC_KEYCODE_q = SDLK_q,
    SC_KEYCODE_r = SDLK_r,
    SC_KEYCODE_s = SDLK_s,
    SC_KEYCODE_t = SDLK_t,
    SC_KEYCODE_u = SDLK_u,
    SC_KEYCODE_v = SDLK_v,
    SC_KEYCODE_w = SDLK_w,
    SC_KEYCODE_x = SDLK_x,
    SC_KEYCODE_y = SDLK_y,
    SC_KEYCODE_z = SDLK_z,

    SC_KEYCODE_CAPSLOCK = SDLK_CAPSLOCK,

    SC_KEYCODE_F1 = SDLK_F1,
    SC_KEYCODE_F2 = SDLK_F2,
    SC_KEYCODE_F3 = SDLK_F3,
    SC_KEYCODE_F4 = SDLK_F4,
    SC_KEYCODE_F5 = SDLK_F5,
    SC_KEYCODE_F6 = SDLK_F6,
    SC_KEYCODE_F7 = SDLK_F7,
    SC_KEYCODE_F8 = SDLK_F8,
    SC_KEYCODE_F9 = SDLK_F9,
    SC_KEYCODE_F10 = SDLK_F10,
    SC_KEYCODE_F11 = SDLK_F11,
    SC_KEYCODE_F12 = SDLK_F12,

    SC_KEYCODE_PRINTSCREEN = SDLK_PRINTSCREEN,
    SC_KEYCODE_SCROLLLOCK = SDLK_SCROLLLOCK,
    SC_KEYCODE_PAUSE = SDLK_PAUSE,
    SC_KEYCODE_INSERT = SDLK_INSERT,
    SC_KEYCODE_HOME = SDLK_HOME,
    SC_KEYCODE_PAGEUP = SDLK_PAGEUP,
    SC_KEYCODE_DELETE = SDLK_DELETE,
    SC_KEYCODE_END = SDLK_END,
    SC_KEYCODE_PAGEDOWN = SDLK_PAGEDOWN,
    SC_KEYCODE_RIGHT = SDLK_RIGHT,
    SC_KEYCODE_LEFT = SDLK_LEFT,
    SC_KEYCODE_DOWN = SDLK_DOWN,
    SC_KEYCODE_UP = SDLK_UP,

    SC_KEYCODE_KP_DIVIDE = SDLK_KP_DIVIDE,
    SC_KEYCODE_KP_MULTIPLY = SDLK_KP_MULTIPLY,
    SC_KEYCODE_KP_MINUS = SDLK_KP_MINUS,
    SC_KEYCODE_KP_PLUS = SDLK_KP_PLUS,
    SC_KEYCODE_KP_ENTER = SDLK_KP_ENTER,
    SC_KEYCODE_KP_1 = SDLK_KP_1,
    SC_KEYCODE_KP_2 = SDLK_KP_2,
    SC_KEYCODE_KP_3 = SDLK_KP_3,
    SC_KEYCODE_KP_4 = SDLK_KP_4,
    SC_KEYCODE_KP_5 = SDLK_KP_5,
    SC_KEYCODE_KP_6 = SDLK_KP_6,
    SC_KEYCODE_KP_7 = SDLK_KP_7,
    SC_KEYCODE_KP_8 = SDLK_KP_8,
    SC_KEYCODE_KP_9 = SDLK_KP_9,
    SC_KEYCODE_KP_0 = SDLK_KP_0,
    SC_KEYCODE_KP_PERIOD = SDLK_KP_PERIOD,
    SC_KEYCODE_KP_EQUALS = SDLK_KP_EQUALS,
    SC_KEYCODE_KP_LEFTPAREN = SDLK_KP_LEFTPAREN,
    SC_KEYCODE_KP_RIGHTPAREN = SDLK_KP_RIGHTPAREN,

    SC_KEYCODE_LCTRL = SDLK_LCTRL,
    SC_KEYCODE_LSHIFT = SDLK_LSHIFT,
    SC_KEYCODE_LALT = SDLK_LALT,
    SC_KEYCODE_LGUI = SDLK_LGUI,
    SC_KEYCODE_RCTRL = SDLK_RCTRL,
    SC_KEYCODE_RSHIFT = SDLK_RSHIFT,
    SC_KEYCODE_RALT = SDLK_RALT,
    SC_KEYCODE_RGUI = SDLK_RGUI,
};

enum sc_scancode {
    SC_SCANCODE_UNKNOWN = SDL_SCANCODE_UNKNOWN,

    SC_SCANCODE_A = SDL_SCANCODE_A,
    SC_SCANCODE_B = SDL_SCANCODE_B,
    SC_SCANCODE_C = SDL_SCANCODE_C,
    SC_SCANCODE_D = SDL_SCANCODE_D,
    SC_SCANCODE_E = SDL_SCANCODE_E,
    SC_SCANCODE_F = SDL_SCANCODE_F,
    SC_SCANCODE_G = SDL_SCANCODE_G,
    SC_SCANCODE_H = SDL_SCANCODE_H,
    SC_SCANCODE_I = SDL_SCANCODE_I,
    SC_SCANCODE_J = SDL_SCANCODE_J,
    SC_SCANCODE_K = SDL_SCANCODE_K,
    SC_SCANCODE_L = SDL_SCANCODE_L,
    SC_SCANCODE_M = SDL_SCANCODE_M,
    SC_SCANCODE_N = SDL_SCANCODE_N,
    SC_SCANCODE_O = SDL_SCANCODE_O,
    SC_SCANCODE_P = SDL_SCANCODE_P,
    SC_SCANCODE_Q = SDL_SCANCODE_Q,
    SC_SCANCODE_R = SDL_SCANCODE_R,
    SC_SCANCODE_S = SDL_SCANCODE_S,
    SC_SCANCODE_T = SDL_SCANCODE_T,
    SC_SCANCODE_U = SDL_SCANCODE_U,
    SC_SCANCODE_V = SDL_SCANCODE_V,
    SC_SCANCODE_W = SDL_SCANCODE_W,
    SC_SCANCODE_X = SDL_SCANCODE_X,
    SC_SCANCODE_Y = SDL_SCANCODE_Y,
    SC_SCANCODE_Z = SDL_SCANCODE_Z,

    SC_SCANCODE_1 = SDL_SCANCODE_1,
    SC_SCANCODE_2 = SDL_SCANCODE_2,
    SC_SCANCODE_3 = SDL_SCANCODE_3,
    SC_SCANCODE_4 = SDL_SCANCODE_4,
    SC_SCANCODE_5 = SDL_SCANCODE_5,
    SC_SCANCODE_6 = SDL_SCANCODE_6,
    SC_SCANCODE_7 = SDL_SCANCODE_7,
    SC_SCANCODE_8 = SDL_SCANCODE_8,
    SC_SCANCODE_9 = SDL_SCANCODE_9,
    SC_SCANCODE_0 = SDL_SCANCODE_0,

    SC_SCANCODE_RETURN = SDL_SCANCODE_RETURN,
    SC_SCANCODE_ESCAPE = SDL_SCANCODE_ESCAPE,
    SC_SCANCODE_BACKSPACE = SDL_SCANCODE_BACKSPACE,
    SC_SCANCODE_TAB = SDL_SCANCODE_TAB,
    SC_SCANCODE_SPACE = SDL_SCANCODE_SPACE,

    SC_SCANCODE_MINUS = SDL_SCANCODE_MINUS,
    SC_SCANCODE_EQUALS = SDL_SCANCODE_EQUALS,
    SC_SCANCODE_LEFTBRACKET = SDL_SCANCODE_LEFTBRACKET,
    SC_SCANCODE_RIGHTBRACKET = SDL_SCANCODE_RIGHTBRACKET,
    SC_SCANCODE_BACKSLASH = SDL_SCANCODE_BACKSLASH,
    SC_SCANCODE_NONUSHASH = SDL_SCANCODE_NONUSHASH,
    SC_SCANCODE_SEMICOLON = SDL_SCANCODE_SEMICOLON,
    SC_SCANCODE_APOSTROPHE = SDL_SCANCODE_APOSTROPHE,
    SC_SCANCODE_GRAVE = SDL_SCANCODE_GRAVE,
    SC_SCANCODE_COMMA = SDL_SCANCODE_COMMA,
    SC_SCANCODE_PERIOD = SDL_SCANCODE_PERIOD,
    SC_SCANCODE_SLASH = SDL_SCANCODE_SLASH,

    SC_SCANCODE_CAPSLOCK = SDL_SCANCODE_CAPSLOCK,

    SC_SCANCODE_F1 = SDL_SCANCODE_F1,
    SC_SCANCODE_F2 = SDL_SCANCODE_F2,
    SC_SCANCODE_F3 = SDL_SCANCODE_F3,
    SC_SCANCODE_F4 = SDL_SCANCODE_F4,
    SC_SCANCODE_F5 = SDL_SCANCODE_F5,
    SC_SCANCODE_F6 = SDL_SCANCODE_F6,
    SC_SCANCODE_F7 = SDL_SCANCODE_F7,
    SC_SCANCODE_F8 = SDL_SCANCODE_F8,
    SC_SCANCODE_F9 = SDL_SCANCODE_F9,
    SC_SCANCODE_F10 = SDL_SCANCODE_F10,
    SC_SCANCODE_F11 = SDL_SCANCODE_F11,
    SC_SCANCODE_F12 = SDL_SCANCODE_F12,

    SC_SCANCODE_PRINTSCREEN = SDL_SCANCODE_PRINTSCREEN,
    SC_SCANCODE_SCROLLLOCK = SDL_SCANCODE_SCROLLLOCK,
    SC_SCANCODE_PAUSE = SDL_SCANCODE_PAUSE,
    SC_SCANCODE_INSERT = SDL_SCANCODE_INSERT,
    SC_SCANCODE_HOME = SDL_SCANCODE_HOME,
    SC_SCANCODE_PAGEUP = SDL_SCANCODE_PAGEUP,
    SC_SCANCODE_DELETE = SDL_SCANCODE_DELETE,
    SC_SCANCODE_END = SDL_SCANCODE_END,
    SC_SCANCODE_PAGEDOWN = SDL_SCANCODE_PAGEDOWN,
    SC_SCANCODE_RIGHT = SDL_SCANCODE_RIGHT,
    SC_SCANCODE_LEFT = SDL_SCANCODE_LEFT,
    SC_SCANCODE_DOWN = SDL_SCANCODE_DOWN,
    SC_SCANCODE_UP = SDL_SCANCODE_UP,

    SC_SCANCODE_NUMLOCK = SDL_SCANCODE_NUMLOCKCLEAR,
    SC_SCANCODE_KP_DIVIDE = SDL_SCANCODE_KP_DIVIDE,
    SC_SCANCODE_KP_MULTIPLY = SDL_SCANCODE_KP_MULTIPLY,
    SC_SCANCODE_KP_MINUS = SDL_SCANCODE_KP_MINUS,
    SC_SCANCODE_KP_PLUS = SDL_SCANCODE_KP_PLUS,
    SC_SCANCODE_KP_ENTER = SDL_SCANCODE_KP_ENTER,
    SC_SCANCODE_KP_1 = SDL_SCANCODE_KP_1,
    SC_SCANCODE_KP_2 = SDL_SCANCODE_KP_2,
    SC_SCANCODE_KP_3 = SDL_SCANCODE_KP_3,
    SC_SCANCODE_KP_4 = SDL_SCANCODE_KP_4,
    SC_SCANCODE_KP_5 = SDL_SCANCODE_KP_5,
    SC_SCANCODE_KP_6 = SDL_SCANCODE_KP_6,
    SC_SCANCODE_KP_7 = SDL_SCANCODE_KP_7,
    SC_SCANCODE_KP_8 = SDL_SCANCODE_KP_8,
    SC_SCANCODE_KP_9 = SDL_SCANCODE_KP_9,
    SC_SCANCODE_KP_0 = SDL_SCANCODE_KP_0,
    SC_SCANCODE_KP_PERIOD = SDL_SCANCODE_KP_PERIOD,

    SC_SCANCODE_LCTRL = SDL_SCANCODE_LCTRL,
    SC_SCANCODE_LSHIFT = SDL_SCANCODE_LSHIFT,
    SC_SCANCODE_LALT = SDL_SCANCODE_LALT,
    SC_SCANCODE_LGUI = SDL_SCANCODE_LGUI,
    SC_SCANCODE_RCTRL = SDL_SCANCODE_RCTRL,
    SC_SCANCODE_RSHIFT = SDL_SCANCODE_RSHIFT,
    SC_SCANCODE_RALT = SDL_SCANCODE_RALT,
    SC_SCANCODE_RGUI = SDL_SCANCODE_RGUI,
};

// On purpose, only use the "mask" values (1, 2, 4, 8, 16) for a single button,
// to avoid unnecessary conversions (and confusion).
enum sc_mouse_button {
    SC_MOUSE_BUTTON_UNKNOWN = 0,
    SC_MOUSE_BUTTON_LEFT = SDL_BUTTON(SDL_BUTTON_LEFT),
    SC_MOUSE_BUTTON_RIGHT = SDL_BUTTON(SDL_BUTTON_RIGHT),
    SC_MOUSE_BUTTON_MIDDLE = SDL_BUTTON(SDL_BUTTON_MIDDLE),
    SC_MOUSE_BUTTON_X1 = SDL_BUTTON(SDL_BUTTON_X1),
    SC_MOUSE_BUTTON_X2 = SDL_BUTTON(SDL_BUTTON_X2),
};

// Use the naming from SDL3 for gamepad axis and buttons:
// <https://wiki.libsdl.org/SDL3/README/migration>

enum sc_gamepad_axis {
    SC_GAMEPAD_AXIS_UNKNOWN = -1,
    SC_GAMEPAD_AXIS_LEFTX = SDL_CONTROLLER_AXIS_LEFTX,
    SC_GAMEPAD_AXIS_LEFTY = SDL_CONTROLLER_AXIS_LEFTY,
    SC_GAMEPAD_AXIS_RIGHTX = SDL_CONTROLLER_AXIS_RIGHTX,
    SC_GAMEPAD_AXIS_RIGHTY = SDL_CONTROLLER_AXIS_RIGHTY,
    SC_GAMEPAD_AXIS_LEFT_TRIGGER = SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SC_GAMEPAD_AXIS_RIGHT_TRIGGER = SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
};

enum sc_gamepad_button {
    SC_GAMEPAD_BUTTON_UNKNOWN = -1,
    SC_GAMEPAD_BUTTON_SOUTH = SDL_CONTROLLER_BUTTON_A,
    SC_GAMEPAD_BUTTON_EAST = SDL_CONTROLLER_BUTTON_B,
    SC_GAMEPAD_BUTTON_WEST = SDL_CONTROLLER_BUTTON_X,
    SC_GAMEPAD_BUTTON_NORTH = SDL_CONTROLLER_BUTTON_Y,
    SC_GAMEPAD_BUTTON_BACK = SDL_CONTROLLER_BUTTON_BACK,
    SC_GAMEPAD_BUTTON_GUIDE = SDL_CONTROLLER_BUTTON_GUIDE,
    SC_GAMEPAD_BUTTON_START = SDL_CONTROLLER_BUTTON_START,
    SC_GAMEPAD_BUTTON_LEFT_STICK = SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SC_GAMEPAD_BUTTON_RIGHT_STICK = SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SC_GAMEPAD_BUTTON_LEFT_SHOULDER = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SC_GAMEPAD_BUTTON_RIGHT_SHOULDER = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SC_GAMEPAD_BUTTON_DPAD_UP = SDL_CONTROLLER_BUTTON_DPAD_UP,
    SC_GAMEPAD_BUTTON_DPAD_DOWN = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SC_GAMEPAD_BUTTON_DPAD_LEFT = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SC_GAMEPAD_BUTTON_DPAD_RIGHT = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
};

static_assert(sizeof(enum sc_mod) >= sizeof(SDL_Keymod),
              "SDL_Keymod must be convertible to sc_mod");

static_assert(sizeof(enum sc_keycode) >= sizeof(SDL_Keycode),
              "SDL_Keycode must be convertible to sc_keycode");

static_assert(sizeof(enum sc_scancode) >= sizeof(SDL_Scancode),
              "SDL_Scancode must be convertible to sc_scancode");

enum sc_touch_action {
    SC_TOUCH_ACTION_MOVE,
    SC_TOUCH_ACTION_DOWN,
    SC_TOUCH_ACTION_UP,
};

struct sc_key_event {
    enum sc_action action;
    enum sc_keycode keycode;
    enum sc_scancode scancode;
    uint16_t mods_state; // bitwise-OR of sc_mod values
    bool repeat;
};

struct sc_text_event {
    const char *text; // not owned
};

struct sc_mouse_click_event {
    struct sc_position position;
    enum sc_action action;
    enum sc_mouse_button button;
    uint64_t pointer_id;
    uint8_t buttons_state; // bitwise-OR of sc_mouse_button values
};

struct sc_mouse_scroll_event {
    struct sc_position position;
    float hscroll;
    float vscroll;
    uint8_t buttons_state; // bitwise-OR of sc_mouse_button values
};

struct sc_mouse_motion_event {
    struct sc_position position;
    uint64_t pointer_id;
    int32_t xrel;
    int32_t yrel;
    uint8_t buttons_state; // bitwise-OR of sc_mouse_button values
};

struct sc_touch_event {
    struct sc_position position;
    enum sc_touch_action action;
    uint64_t pointer_id;
    float pressure;
};

// As documented in <https://wiki.libsdl.org/SDL2/SDL_JoystickID>:
// The ID value starts at 0 and increments from there. The value -1 is an
// invalid ID.
#define SC_GAMEPAD_ID_INVALID UINT32_C(-1)

struct sc_gamepad_device_event {
    uint32_t gamepad_id;
};

struct sc_gamepad_button_event {
    uint32_t gamepad_id;
    enum sc_action action;
    enum sc_gamepad_button button;
};

struct sc_gamepad_axis_event {
    uint32_t gamepad_id;
    enum sc_gamepad_axis axis;
    int16_t value;
};

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

static inline enum sc_touch_action
sc_touch_action_from_sdl(uint32_t type) {
    assert(type == SDL_FINGERMOTION || type == SDL_FINGERDOWN ||
           type == SDL_FINGERUP);
    if (type == SDL_FINGERMOTION) {
        return SC_TOUCH_ACTION_MOVE;
    }
    if (type == SDL_FINGERDOWN) {
        return SC_TOUCH_ACTION_DOWN;
    }
    return SC_TOUCH_ACTION_UP;
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

    // SC_MOUSE_BUTTON_* constants are initialized from SDL_BUTTON(index)
    return buttons_state;
}

static inline enum sc_gamepad_axis
sc_gamepad_axis_from_sdl(uint8_t axis) {
    if (axis <= SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        // SC_GAMEPAD_AXIS_* constants are initialized from
        // SDL_CONTROLLER_AXIS_*
        return axis;
    }
    return SC_GAMEPAD_AXIS_UNKNOWN;
}

static inline enum sc_gamepad_button
sc_gamepad_button_from_sdl(uint8_t button) {
    if (button <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
        // SC_GAMEPAD_BUTTON_* constants are initialized from
        // SDL_CONTROLLER_BUTTON_*
        return button;
    }
    return SC_GAMEPAD_BUTTON_UNKNOWN;
}

static inline enum sc_action
sc_action_from_sdl_controllerbutton_type(uint32_t type) {
    assert(type == SDL_CONTROLLERBUTTONDOWN || type == SDL_CONTROLLERBUTTONUP);
    if (type == SDL_CONTROLLERBUTTONDOWN) {
        return SC_ACTION_DOWN;
    }
    return SC_ACTION_UP;
}

#endif
