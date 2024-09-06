#include "screen_otg.h"

#include "icon.h"
#include "options.h"
#include "util/log.h"

static void
sc_screen_otg_set_mouse_capture(struct sc_screen_otg *screen, bool capture) {
#ifdef __APPLE__
    // Workaround for SDL bug on macOS:
    // <https://github.com/libsdl-org/SDL/issues/5340>
    if (capture) {
        int mouse_x, mouse_y;
        SDL_GetGlobalMouseState(&mouse_x, &mouse_y);

        int x, y, w, h;
        SDL_GetWindowPosition(screen->window, &x, &y);
        SDL_GetWindowSize(screen->window, &w, &h);

        bool outside_window = mouse_x < x || mouse_x >= x + w
                           || mouse_y < y || mouse_y >= y + h;
        if (outside_window) {
            SDL_WarpMouseInWindow(screen->window, w / 2, h / 2);
        }
    }
#else
    (void) screen;
#endif
    if (SDL_SetRelativeMouseMode(capture)) {
        LOGE("Could not set relative mouse mode to %s: %s",
             capture ? "true" : "false", SDL_GetError());
    }
}

static inline bool
sc_screen_otg_get_mouse_capture(struct sc_screen_otg *screen) {
    (void) screen;
    return SDL_GetRelativeMouseMode();
}

static inline void
sc_screen_otg_toggle_mouse_capture(struct sc_screen_otg *screen) {
    (void) screen;
    bool new_value = !sc_screen_otg_get_mouse_capture(screen);
    sc_screen_otg_set_mouse_capture(screen, new_value);
}

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

    screen->mouse_capture_key_pressed = 0;

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

    if (screen->mouse) {
        // Capture mouse on start
        sc_screen_otg_set_mouse_capture(screen, true);
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

static inline bool
sc_screen_otg_is_mouse_capture_key(SDL_Keycode key) {
    return key == SDLK_LALT || key == SDLK_LGUI || key == SDLK_RGUI;
}

static void
sc_screen_otg_process_key(struct sc_screen_otg *screen,
                             const SDL_KeyboardEvent *event) {
    assert(screen->keyboard);
    struct sc_key_processor *kp = &screen->keyboard->key_processor;

    struct sc_key_event evt = {
        .action = sc_action_from_sdl_keyboard_type(event->type),
        .keycode = sc_keycode_from_sdl(event->keysym.sym),
        .scancode = sc_scancode_from_sdl(event->keysym.scancode),
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
        .hscroll = event->x,
        .vscroll = event->y,
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

    SDL_JoystickID id;
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

        id = SDL_JoystickInstanceID(joystick);
    } else if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        id = event->which;

        SDL_GameController *gc = SDL_GameControllerFromInstanceID(id);
        if (gc) {
            SDL_GameControllerClose(gc);
        } else {
            LOGW("Unknown gamepad device removed");
        }
    } else {
        // Nothing to do
        return;
    }

    struct sc_gamepad_device_event evt = {
        .type = sc_gamepad_device_event_type_from_sdl_type(event->type),
        .gamepad_id = id,
    };
    gp->ops->process_gamepad_device(gp, &evt);
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
    switch (event->type) {
        case SDL_WINDOWEVENT:
            switch (event->window.event) {
                case SDL_WINDOWEVENT_EXPOSED:
                    sc_screen_otg_render(screen);
                    break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    if (screen->mouse) {
                        sc_screen_otg_set_mouse_capture(screen, false);
                    }
                    break;
            }
            return;
        case SDL_KEYDOWN:
            if (screen->mouse) {
                SDL_Keycode key = event->key.keysym.sym;
                if (sc_screen_otg_is_mouse_capture_key(key)) {
                    if (!screen->mouse_capture_key_pressed) {
                        screen->mouse_capture_key_pressed = key;
                    } else {
                        // Another mouse capture key has been pressed, cancel
                        // mouse (un)capture
                        screen->mouse_capture_key_pressed = 0;
                    }
                    // Mouse capture keys are never forwarded to the device
                    return;
                }
            }

            if (screen->keyboard) {
                sc_screen_otg_process_key(screen, &event->key);
            }
            break;
        case SDL_KEYUP:
            if (screen->mouse) {
                SDL_Keycode key = event->key.keysym.sym;
                SDL_Keycode cap = screen->mouse_capture_key_pressed;
                screen->mouse_capture_key_pressed = 0;
                if (sc_screen_otg_is_mouse_capture_key(key)) {
                    if (key == cap) {
                        // A mouse capture key has been pressed then released:
                        // toggle the capture mouse mode
                        sc_screen_otg_toggle_mouse_capture(screen);
                    }
                    // Mouse capture keys are never forwarded to the device
                    return;
                }
            }

            if (screen->keyboard) {
                sc_screen_otg_process_key(screen, &event->key);
            }
            break;
        case SDL_MOUSEMOTION:
            if (screen->mouse && sc_screen_otg_get_mouse_capture(screen)) {
                sc_screen_otg_process_mouse_motion(screen, &event->motion);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (screen->mouse && sc_screen_otg_get_mouse_capture(screen)) {
                sc_screen_otg_process_mouse_button(screen, &event->button);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (screen->mouse) {
                if (sc_screen_otg_get_mouse_capture(screen)) {
                    sc_screen_otg_process_mouse_button(screen, &event->button);
                } else {
                    sc_screen_otg_set_mouse_capture(screen, true);
                }
            }
            break;
        case SDL_MOUSEWHEEL:
            if (screen->mouse && sc_screen_otg_get_mouse_capture(screen)) {
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
