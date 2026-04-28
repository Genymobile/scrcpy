#include "screen_otg.h"

#include <assert.h>
#include <stddef.h>

#include "icon.h"
#include "options.h"
#include "util/acksync.h"
#include "util/log.h"

static void
sc_screen_otg_render(struct sc_screen_otg *screen) {
    SDL_RenderClear(screen->renderer);
    if (screen->texture) {
        SDL_RenderCopy(screen->renderer, screen->texture, NULL, NULL);
    }
    SDL_RenderPresent(screen->renderer);
}

bool
sc_screen_otg_init(struct sc_screen_otg *screen,
                   const struct sc_screen_otg_params *params) {
    screen->keyboard = params->keyboard;
    screen->mouse = params->mouse;
    screen->gamepad = params->gamepad;

    const char *title = params->window_title;
    assert(title);

    int x = params->window_x != SC_WINDOW_POSITION_UNDEFINED
          ? params->window_x : (int) SDL_WINDOWPOS_UNDEFINED;
    int y = params->window_y != SC_WINDOW_POSITION_UNDEFINED
          ? params->window_y : (int) SDL_WINDOWPOS_UNDEFINED;
    int width = params->window_width ? params->window_width : 256;
    int height = params->window_height ? params->window_height : 256;

    uint32_t window_flags = SDL_WINDOW_ALLOW_HIGHDPI;
    if (params->always_on_top) {
        window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    if (params->window_borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    screen->window = SDL_CreateWindow(title, x, y, width, height, window_flags);
    if (!screen->window) {
        LOGE("Could not create window: %s", SDL_GetError());
        return false;
    }

    screen->renderer = SDL_CreateRenderer(screen->window, -1, 0);
    if (!screen->renderer) {
        LOGE("Could not create renderer: %s", SDL_GetError());
        goto error_destroy_window;
    }

    SDL_Surface *icon = scrcpy_icon_load();

    if (icon) {
        SDL_SetWindowIcon(screen->window, icon);

        if (SDL_RenderSetLogicalSize(screen->renderer, icon->w, icon->h)) {
            LOGW("Could not set renderer logical size: %s", SDL_GetError());
            // don't fail
        }

        screen->texture = SDL_CreateTextureFromSurface(screen->renderer, icon);
        scrcpy_icon_destroy(icon);
        if (!screen->texture) {
            goto error_destroy_renderer;
        }
    } else {
        screen->texture = NULL;
        LOGW("Could not load icon");
    }

    sc_mouse_capture_init(&screen->mc, screen->window, params->shortcut_mods);

    if (screen->mouse) {
        // Capture mouse on start
        sc_mouse_capture_set_active(&screen->mc, true);
    }

    return true;

error_destroy_window:
    SDL_DestroyWindow(screen->window);
error_destroy_renderer:
    SDL_DestroyRenderer(screen->renderer);

    return false;
}

void
sc_screen_otg_destroy(struct sc_screen_otg *screen) {
    if (screen->texture) {
        SDL_DestroyTexture(screen->texture);
    }
    SDL_DestroyRenderer(screen->renderer);
    SDL_DestroyWindow(screen->window);
}

static const enum sc_scancode keycode_to_scancode[] = {
    /* SDL2 has SDL_GetScancodeFromKey, but that uses the current keymap */

    [SC_KEYCODE_RETURN] = SC_SCANCODE_RETURN,
    [SC_KEYCODE_ESCAPE] = SC_SCANCODE_ESCAPE,
    [SC_KEYCODE_BACKSPACE] = SC_SCANCODE_BACKSPACE,
    [SC_KEYCODE_TAB] = SC_SCANCODE_TAB,
    [SC_KEYCODE_SPACE] = SC_SCANCODE_SPACE,
    [SC_KEYCODE_EXCLAIM] = SC_SCANCODE_1,
    [SC_KEYCODE_QUOTEDBL] = SC_SCANCODE_APOSTROPHE,
    [SC_KEYCODE_HASH] = SC_SCANCODE_3,
    [SC_KEYCODE_PERCENT] = SC_SCANCODE_5,
    [SC_KEYCODE_DOLLAR] = SC_SCANCODE_4,
    [SC_KEYCODE_AMPERSAND] = SC_SCANCODE_7,
    [SC_KEYCODE_QUOTE] = SC_SCANCODE_APOSTROPHE,
    [SC_KEYCODE_LEFTPAREN] = SC_SCANCODE_9,
    [SC_KEYCODE_RIGHTPAREN] = SC_SCANCODE_0,
    [SC_KEYCODE_ASTERISK] = SC_SCANCODE_8,
    [SC_KEYCODE_PLUS] = SC_SCANCODE_EQUALS,
    [SC_KEYCODE_COMMA] = SC_SCANCODE_COMMA,
    [SC_KEYCODE_MINUS] = SC_SCANCODE_MINUS,
    [SC_KEYCODE_PERIOD] = SC_SCANCODE_PERIOD,
    [SC_KEYCODE_SLASH] = SC_SCANCODE_SLASH,
    [SC_KEYCODE_0] = SC_SCANCODE_0,
    [SC_KEYCODE_1] = SC_SCANCODE_1,
    [SC_KEYCODE_2] = SC_SCANCODE_2,
    [SC_KEYCODE_3] = SC_SCANCODE_3,
    [SC_KEYCODE_4] = SC_SCANCODE_4,
    [SC_KEYCODE_5] = SC_SCANCODE_5,
    [SC_KEYCODE_6] = SC_SCANCODE_6,
    [SC_KEYCODE_7] = SC_SCANCODE_7,
    [SC_KEYCODE_8] = SC_SCANCODE_8,
    [SC_KEYCODE_9] = SC_SCANCODE_9,
    [SC_KEYCODE_COLON] = SC_SCANCODE_SEMICOLON,
    [SC_KEYCODE_SEMICOLON] = SC_SCANCODE_SEMICOLON,
    [SC_KEYCODE_LESS] = SC_SCANCODE_COMMA,
    [SC_KEYCODE_EQUALS] = SC_SCANCODE_EQUALS,
    [SC_KEYCODE_GREATER] = SC_SCANCODE_PERIOD,
    [SC_KEYCODE_QUESTION] = SC_SCANCODE_SLASH,
    [SC_KEYCODE_AT] = SC_SCANCODE_2,

    [SC_KEYCODE_LEFTBRACKET] = SC_SCANCODE_LEFTBRACKET,
    [SC_KEYCODE_BACKSLASH] = SC_SCANCODE_BACKSLASH,
    [SC_KEYCODE_RIGHTBRACKET] = SC_SCANCODE_RIGHTBRACKET,
    [SC_KEYCODE_CARET] = SC_SCANCODE_6,
    [SC_KEYCODE_UNDERSCORE] = SC_SCANCODE_MINUS,
    [SC_KEYCODE_BACKQUOTE] = SC_SCANCODE_GRAVE,
    [SC_KEYCODE_a] = SC_SCANCODE_A,
    [SC_KEYCODE_b] = SC_SCANCODE_B,
    [SC_KEYCODE_c] = SC_SCANCODE_C,
    [SC_KEYCODE_d] = SC_SCANCODE_D,
    [SC_KEYCODE_e] = SC_SCANCODE_E,
    [SC_KEYCODE_f] = SC_SCANCODE_F,
    [SC_KEYCODE_g] = SC_SCANCODE_G,
    [SC_KEYCODE_h] = SC_SCANCODE_H,
    [SC_KEYCODE_i] = SC_SCANCODE_I,
    [SC_KEYCODE_j] = SC_SCANCODE_J,
    [SC_KEYCODE_k] = SC_SCANCODE_K,
    [SC_KEYCODE_l] = SC_SCANCODE_L,
    [SC_KEYCODE_m] = SC_SCANCODE_M,
    [SC_KEYCODE_n] = SC_SCANCODE_N,
    [SC_KEYCODE_o] = SC_SCANCODE_O,
    [SC_KEYCODE_p] = SC_SCANCODE_P,
    [SC_KEYCODE_q] = SC_SCANCODE_Q,
    [SC_KEYCODE_r] = SC_SCANCODE_R,
    [SC_KEYCODE_s] = SC_SCANCODE_S,
    [SC_KEYCODE_t] = SC_SCANCODE_T,
    [SC_KEYCODE_u] = SC_SCANCODE_U,
    [SC_KEYCODE_v] = SC_SCANCODE_V,
    [SC_KEYCODE_w] = SC_SCANCODE_W,
    [SC_KEYCODE_x] = SC_SCANCODE_X,
    [SC_KEYCODE_y] = SC_SCANCODE_Y,
    [SC_KEYCODE_z] = SC_SCANCODE_Z,
};

static void
sc_screen_otg_process_key(struct sc_screen_otg *screen,
                             const SDL_KeyboardEvent *event) {
    assert(screen->keyboard);
    struct sc_key_processor *kp = &screen->keyboard->key_processor;

    enum sc_keycode keycode = sc_keycode_from_sdl(event->keysym.sym);
    enum sc_scancode scancode = sc_scancode_from_sdl(event->keysym.scancode);

    if (kp->use_logical_scancodes && keycode < ARRAY_LEN(keycode_to_scancode)) {
        enum sc_scancode logical_scancode = keycode_to_scancode[keycode];
        if (logical_scancode != 0) {
            scancode = logical_scancode;
        }
    }

    struct sc_key_event evt = {
        .action = sc_action_from_sdl_keyboard_type(event->type),
        .keycode = keycode,
        .scancode = scancode,
        .repeat = event->repeat,
        .mods_state = sc_mods_state_from_sdl(event->keysym.mod),
    };

    assert(kp->ops->process_key);
    kp->ops->process_key(kp, &evt, SC_SEQUENCE_INVALID);
}

static void
sc_screen_otg_process_mouse_motion(struct sc_screen_otg *screen,
                                   const SDL_MouseMotionEvent *event) {
    assert(screen->mouse);
    struct sc_mouse_processor *mp = &screen->mouse->mouse_processor;

    struct sc_mouse_motion_event evt = {
        // .position not used for HID events
        .xrel = event->xrel,
        .yrel = event->yrel,
        .buttons_state = sc_mouse_buttons_state_from_sdl(event->state),
    };

    assert(mp->ops->process_mouse_motion);
    mp->ops->process_mouse_motion(mp, &evt);
}

static void
sc_screen_otg_process_mouse_button(struct sc_screen_otg *screen,
                                   const SDL_MouseButtonEvent *event) {
    assert(screen->mouse);
    struct sc_mouse_processor *mp = &screen->mouse->mouse_processor;

    uint32_t sdl_buttons_state = SDL_GetMouseState(NULL, NULL);

    struct sc_mouse_click_event evt = {
        // .position not used for HID events
        .action = sc_action_from_sdl_mousebutton_type(event->type),
        .button = sc_mouse_button_from_sdl(event->button),
        .buttons_state = sc_mouse_buttons_state_from_sdl(sdl_buttons_state),
    };

    assert(mp->ops->process_mouse_click);
    mp->ops->process_mouse_click(mp, &evt);
}

static void
sc_screen_otg_process_mouse_wheel(struct sc_screen_otg *screen,
                                  const SDL_MouseWheelEvent *event) {
    assert(screen->mouse);
    struct sc_mouse_processor *mp = &screen->mouse->mouse_processor;

    uint32_t sdl_buttons_state = SDL_GetMouseState(NULL, NULL);

    struct sc_mouse_scroll_event evt = {
        // .position not used for HID events
#if SDL_VERSION_ATLEAST(2, 0, 18)
        .hscroll = event->preciseX,
        .vscroll = event->preciseY,
#else
        .hscroll = event->x,
        .vscroll = event->y,
#endif
        .hscroll_int = event->x,
        .vscroll_int = event->y,
        .buttons_state = sc_mouse_buttons_state_from_sdl(sdl_buttons_state),
    };

    assert(mp->ops->process_mouse_scroll);
    mp->ops->process_mouse_scroll(mp, &evt);
}

static void
sc_screen_otg_process_gamepad_device(struct sc_screen_otg *screen,
                                     const SDL_ControllerDeviceEvent *event) {
    assert(screen->gamepad);
    struct sc_gamepad_processor *gp = &screen->gamepad->gamepad_processor;

    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        SDL_GameController *gc = SDL_GameControllerOpen(event->which);
        if (!gc) {
            LOGW("Could not open game controller");
            return;
        }

        SDL_Joystick *joystick = SDL_GameControllerGetJoystick(gc);
        if (!joystick) {
            LOGW("Could not get controller joystick");
            SDL_GameControllerClose(gc);
            return;
        }

        struct sc_gamepad_device_event evt = {
            .gamepad_id = SDL_JoystickInstanceID(joystick),
        };
        gp->ops->process_gamepad_added(gp, &evt);
    } else if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        SDL_JoystickID id = event->which;

        SDL_GameController *gc = SDL_GameControllerFromInstanceID(id);
        if (gc) {
            SDL_GameControllerClose(gc);
        } else {
            LOGW("Unknown gamepad device removed");
        }

        struct sc_gamepad_device_event evt = {
            .gamepad_id = id,
        };
        gp->ops->process_gamepad_removed(gp, &evt);
    }
}

static void
sc_screen_otg_process_gamepad_axis(struct sc_screen_otg *screen,
                                   const SDL_ControllerAxisEvent *event) {
    assert(screen->gamepad);
    struct sc_gamepad_processor *gp = &screen->gamepad->gamepad_processor;

    enum sc_gamepad_axis axis = sc_gamepad_axis_from_sdl(event->axis);
    if (axis == SC_GAMEPAD_AXIS_UNKNOWN) {
        return;
    }

    struct sc_gamepad_axis_event evt = {
        .gamepad_id = event->which,
        .axis = axis,
        .value = event->value,
    };
    gp->ops->process_gamepad_axis(gp, &evt);
}

static void
sc_screen_otg_process_gamepad_button(struct sc_screen_otg *screen,
                                     const SDL_ControllerButtonEvent *event) {
    assert(screen->gamepad);
    struct sc_gamepad_processor *gp = &screen->gamepad->gamepad_processor;

    enum sc_gamepad_button button = sc_gamepad_button_from_sdl(event->button);
    if (button == SC_GAMEPAD_BUTTON_UNKNOWN) {
        return;
    }

    struct sc_gamepad_button_event evt = {
        .gamepad_id = event->which,
        .action = sc_action_from_sdl_controllerbutton_type(event->type),
        .button = button,
    };
    gp->ops->process_gamepad_button(gp, &evt);
}

void
sc_screen_otg_handle_event(struct sc_screen_otg *screen, SDL_Event *event) {
    if (sc_mouse_capture_handle_event(&screen->mc, event)) {
        // The mouse capture handler consumed the event
        return;
    }

    switch (event->type) {
        case SDL_WINDOWEVENT:
            switch (event->window.event) {
                case SDL_WINDOWEVENT_EXPOSED:
                    sc_screen_otg_render(screen);
                    break;
            }
            return;
        case SDL_KEYDOWN:
            if (screen->keyboard) {
                sc_screen_otg_process_key(screen, &event->key);
            }
            break;
        case SDL_KEYUP:
            if (screen->keyboard) {
                sc_screen_otg_process_key(screen, &event->key);
            }
            break;
        case SDL_MOUSEMOTION:
            if (screen->mouse) {
                sc_screen_otg_process_mouse_motion(screen, &event->motion);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (screen->mouse) {
                sc_screen_otg_process_mouse_button(screen, &event->button);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (screen->mouse) {
                sc_screen_otg_process_mouse_button(screen, &event->button);
            }
            break;
        case SDL_MOUSEWHEEL:
            if (screen->mouse) {
                sc_screen_otg_process_mouse_wheel(screen, &event->wheel);
            }
            break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            // Handle device added or removed even if paused
            if (screen->gamepad) {
                sc_screen_otg_process_gamepad_device(screen, &event->cdevice);
            }
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (screen->gamepad) {
                sc_screen_otg_process_gamepad_axis(screen, &event->caxis);
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            if (screen->gamepad) {
                sc_screen_otg_process_gamepad_button(screen, &event->cbutton);
            }
            break;
    }
}
