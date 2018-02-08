#include "screencontrol.h"

#include <SDL2/SDL_log.h>

#include "convert.h"

static struct point get_mouse_point(void) {
    int x;
    int y;
    SDL_GetMouseState(&x, &y);
    SDL_assert_release(x >= 0 && x < 0x10000 && y >= 0 && y < 0x10000);
    return (struct point) {
        .x = (Uint16) x,
        .y = (Uint16) y,
    };
}

static SDL_bool is_ctrl_down(void) {
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
}

static void send_keycode(struct controller *controller, enum android_keycode keycode, const char *name) {
    // send DOWN event
    struct control_event control_event = {
        .type = CONTROL_EVENT_TYPE_KEYCODE,
        .keycode_event = {
            .action = AKEY_EVENT_ACTION_DOWN,
            .keycode = keycode,
            .metastate = 0,
        },
    };

    if (!controller_push_event(controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send %s (DOWN)", name);
        return;
    }

    // send UP event
    control_event.keycode_event.action = AKEY_EVENT_ACTION_UP;
    if (!controller_push_event(controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send %s (UP)", name);
    }
}

static inline void action_home(struct controller *controller) {
    send_keycode(controller, AKEYCODE_HOME, "HOME");
}

static inline void action_back(struct controller *controller) {
    send_keycode(controller, AKEYCODE_BACK, "BACK");
}

static inline void action_app_switch(struct controller *controller) {
    send_keycode(controller, AKEYCODE_APP_SWITCH, "APP_SWITCH");
}

static inline void action_power(struct controller *controller) {
    send_keycode(controller, AKEYCODE_POWER, "POWER");
}

static inline void action_volume_up(struct controller *controller) {
    send_keycode(controller, AKEYCODE_VOLUME_UP, "VOLUME_UP");
}

static inline void action_volume_down(struct controller *controller) {
    send_keycode(controller, AKEYCODE_VOLUME_DOWN, "VOLUME_DOWN");
}

static void turn_screen_on(struct controller *controller) {
    struct control_event control_event = {
        .type = CONTROL_EVENT_TYPE_COMMAND,
        .command_event = {
            .action = CONTROL_EVENT_COMMAND_SCREEN_ON,
        },
    };
    if (!controller_push_event(controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot turn screen on");
    }
}

void screencontrol_handle_text_input(struct controller *controller,
                                     struct screen *screen,
                                     const SDL_TextInputEvent *event) {
    if (is_ctrl_down()) {
        switch (event->text[0]) {
            case '+':
                action_volume_up(controller);
                break;
            case '-':
                action_volume_down(controller);
                break;
        }
        return;
    }

    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_TEXT;
    strncpy(control_event.text_event.text, event->text, TEXT_MAX_LENGTH);
    control_event.text_event.text[TEXT_MAX_LENGTH] = '\0';
    if (!controller_push_event(controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send text event");
    }
}

void screencontrol_handle_key(struct controller *controller,
                              struct screen *screen,
                              const SDL_KeyboardEvent *event) {
    SDL_Keycode keycode = event->keysym.sym;
    SDL_bool ctrl = event->keysym.mod & (KMOD_LCTRL | KMOD_RCTRL);
    SDL_bool shift = event->keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT);
    SDL_bool repeat = event->repeat;

    // capture all Ctrl events
    if (ctrl) {
        // only consider keydown events, and ignore repeated events
        if (repeat || event->type != SDL_KEYDOWN) {
            return;
        }

        if (shift) {
            // currently, there is no shortcut implying SHIFT
            return;
        }

        switch (keycode) {
            case SDLK_h:
                action_home(controller);
                return;
            case SDLK_b: // fall-through
            case SDLK_BACKSPACE:
                action_back(controller);
                return;
            case SDLK_m:
                action_app_switch(controller);
                return;
            case SDLK_p:
                action_power(controller);
                return;
            case SDLK_f:
                screen_switch_fullscreen(screen);
                return;
            case SDLK_x:
                screen_resize_to_fit(screen);
                return;
            case SDLK_g:
                screen_resize_to_pixel_perfect(screen);
                return;
        }

        return;
    }

    struct control_event control_event;
    if (input_key_from_sdl_to_android(event, &control_event)) {
        if (!controller_push_event(controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send control event");
        }
    }
}

void screencontrol_handle_mouse_motion(struct controller *controller,
                                       struct screen *screen,
                                       const SDL_MouseMotionEvent *event) {
    if (!event->state) {
        // do not send motion events when no button is pressed
        return;
    }
    struct control_event control_event;
    if (mouse_motion_from_sdl_to_android(event, screen->frame_size, &control_event)) {
        if (!controller_push_event(controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send mouse motion event");
        }
    }
}

void screencontrol_handle_mouse_button(struct controller *controller,
                                       struct screen *screen,
                                       const SDL_MouseButtonEvent *event) {
    if (event->button == SDL_BUTTON_RIGHT) {
        turn_screen_on(controller);
        return;
    };
    struct control_event control_event;
    if (mouse_button_from_sdl_to_android(event, screen->frame_size, &control_event)) {
        if (!controller_push_event(controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send mouse button event");
        }
    }
}

void screencontrol_handle_mouse_wheel(struct controller *controller,
                                      struct screen *screen,
                                      const SDL_MouseWheelEvent *event) {
    struct position position = {
        .screen_size = screen->frame_size,
        .point = get_mouse_point(),
    };
    struct control_event control_event;
    if (mouse_wheel_from_sdl_to_android(event, position, &control_event)) {
        if (!controller_push_event(controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send wheel button event");
        }
    }
}
