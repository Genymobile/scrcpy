#include "input_manager.h"

#include <SDL2/SDL_assert.h>
#include "convert.h"
#include "lock_util.h"
#include "log.h"

// Convert window coordinates (as provided by SDL_GetMouseState() to renderer
// coordinates (as provided in SDL mouse events)
//
// See my question:
// <https://stackoverflow.com/questions/49111054/how-to-get-mouse-position-on-mouse-wheel-event>
static void
convert_to_renderer_coordinates(SDL_Renderer *renderer, int *x, int *y) {
    SDL_Rect viewport;
    float scale_x, scale_y;
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderGetScale(renderer, &scale_x, &scale_y);
    *x = (int) (*x / scale_x) - viewport.x;
    *y = (int) (*y / scale_y) - viewport.y;
}

static struct point
get_mouse_point(struct screen *screen) {
    int x;
    int y;
    SDL_GetMouseState(&x, &y);
    convert_to_renderer_coordinates(screen->renderer, &x, &y);
    return (struct point) {
        .x = x,
        .y = y,
    };
}

static const int ACTION_DOWN = 1;
static const int ACTION_UP = 1 << 1;

static void
send_keycode(struct controller *controller, enum android_keycode keycode,
             int actions, const char *name) {
    // send DOWN event
    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_KEYCODE;
    control_event.keycode_event.keycode = keycode;
    control_event.keycode_event.metastate = 0;

    if (actions & ACTION_DOWN) {
        control_event.keycode_event.action = AKEY_EVENT_ACTION_DOWN;
        if (!controller_push_event(controller, &control_event)) {
            LOGW("Cannot send %s (DOWN)", name);
            return;
        }
    }

    if (actions & ACTION_UP) {
        control_event.keycode_event.action = AKEY_EVENT_ACTION_UP;
        if (!controller_push_event(controller, &control_event)) {
            LOGW("Cannot send %s (UP)", name);
        }
    }
}

static inline void
action_home(struct controller *controller, int actions) {
    send_keycode(controller, AKEYCODE_HOME, actions, "HOME");
}

static inline void
action_back(struct controller *controller, int actions) {
    send_keycode(controller, AKEYCODE_BACK, actions, "BACK");
}

static inline void
action_app_switch(struct controller *controller, int actions) {
    send_keycode(controller, AKEYCODE_APP_SWITCH, actions, "APP_SWITCH");
}

static inline void
action_power(struct controller *controller, int actions) {
    send_keycode(controller, AKEYCODE_POWER, actions, "POWER");
}

static inline void
action_volume_up(struct controller *controller, int actions) {
    send_keycode(controller, AKEYCODE_VOLUME_UP, actions, "VOLUME_UP");
}

static inline void
action_volume_down(struct controller *controller, int actions) {
    send_keycode(controller, AKEYCODE_VOLUME_DOWN, actions, "VOLUME_DOWN");
}

static inline void
action_menu(struct controller *controller, int actions) {
    send_keycode(controller, AKEYCODE_MENU, actions, "MENU");
}

// turn the screen on if it was off, press BACK otherwise
static void
press_back_or_turn_screen_on(struct controller *controller) {
    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_COMMAND;
    control_event.command_event.action =
            CONTROL_EVENT_COMMAND_BACK_OR_SCREEN_ON;

    if (!controller_push_event(controller, &control_event)) {
        LOGW("Cannot turn screen on");
    }
}

static void
expand_notification_panel(struct controller *controller) {
    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_COMMAND;
    control_event.command_event.action =
            CONTROL_EVENT_COMMAND_EXPAND_NOTIFICATION_PANEL;

    if (!controller_push_event(controller, &control_event)) {
        LOGW("Cannot expand notification panel");
    }
}

static void
collapse_notification_panel(struct controller *controller) {
    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_COMMAND;
    control_event.command_event.action =
            CONTROL_EVENT_COMMAND_COLLAPSE_NOTIFICATION_PANEL;

    if (!controller_push_event(controller, &control_event)) {
        LOGW("Cannot collapse notification panel");
    }
}

static void
switch_fps_counter_state(struct video_buffer *vb) {
    mutex_lock(vb->mutex);
    if (vb->fps_counter.started) {
        LOGI("FPS counter stopped");
        fps_counter_stop(&vb->fps_counter);
    } else {
        LOGI("FPS counter started");
        fps_counter_start(&vb->fps_counter);
    }
    mutex_unlock(vb->mutex);
}

static void
clipboard_paste(struct controller *controller) {
    char *text = SDL_GetClipboardText();
    if (!text) {
        LOGW("Cannot get clipboard text: %s", SDL_GetError());
        return;
    }
    if (!*text) {
        // empty text
        SDL_free(text);
        return;
    }

    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_TEXT;
    control_event.text_event.text = text;
    if (!controller_push_event(controller, &control_event)) {
        SDL_free(text);
        LOGW("Cannot send clipboard paste event");
    }
}

void
input_manager_process_text_input(struct input_manager *input_manager,
                                 const SDL_TextInputEvent *event) {
    char c = event->text[0];
    if (isalpha(c) || c == ' ') {
        SDL_assert(event->text[1] == '\0');
        // letters and space are handled as raw key event
        return;
    }
    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_TEXT;
    control_event.text_event.text = SDL_strdup(event->text);
    if (!control_event.text_event.text) {
        LOGW("Cannot strdup input text");
        return;
    }
    if (!controller_push_event(input_manager->controller, &control_event)) {
        SDL_free(control_event.text_event.text);
        LOGW("Cannot send text event");
    }
}

void
input_manager_process_key(struct input_manager *input_manager,
                          const SDL_KeyboardEvent *event,
                          bool control) {
    bool ctrl = event->keysym.mod & (KMOD_LCTRL | KMOD_RCTRL);
    bool alt = event->keysym.mod & (KMOD_LALT | KMOD_RALT);
    bool meta = event->keysym.mod & (KMOD_LGUI | KMOD_RGUI);

    if (alt) {
        // no shortcut involves Alt or Meta, and they should not be forwarded
        // to the device
        return;
    }

    // capture all Ctrl events
    if (ctrl | meta) {
        SDL_Keycode keycode = event->keysym.sym;
        int action = event->type == SDL_KEYDOWN ? ACTION_DOWN : ACTION_UP;
        bool repeat = event->repeat;
        bool shift = event->keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT);
        switch (keycode) {
            case SDLK_h:
                if (control && ctrl && !meta && !shift && !repeat) {
                    action_home(input_manager->controller, action);
                }
                return;
            case SDLK_b: // fall-through
            case SDLK_BACKSPACE:
                if (control && ctrl && !meta && !shift && !repeat) {
                    action_back(input_manager->controller, action);
                }
                return;
            case SDLK_s:
                if (control && ctrl && !meta && !shift && !repeat) {
                    action_app_switch(input_manager->controller, action);
                }
                return;
            case SDLK_m:
                if (control && ctrl && !meta && !shift && !repeat) {
                    action_menu(input_manager->controller, action);
                }
                return;
            case SDLK_p:
                if (control && ctrl && !meta && !shift && !repeat) {
                    action_power(input_manager->controller, action);
                }
                return;
            case SDLK_DOWN:
#ifdef __APPLE__
                if (control && !ctrl && meta && !shift) {
#else
                if (control && ctrl && !meta && !shift) {
#endif
                    // forward repeated events
                    action_volume_down(input_manager->controller, action);
                }
                return;
            case SDLK_UP:
#ifdef __APPLE__
                if (control && !ctrl && meta && !shift) {
#else
                if (control && ctrl && !meta && !shift) {
#endif
                    // forward repeated events
                    action_volume_up(input_manager->controller, action);
                }
                return;
            case SDLK_v:
                if (control && ctrl && !meta && !shift && !repeat
                        && event->type == SDL_KEYDOWN) {
                    clipboard_paste(input_manager->controller);
                }
                return;
            case SDLK_f:
                if (ctrl && !meta && !shift && !repeat
                        && event->type == SDL_KEYDOWN) {
                    screen_switch_fullscreen(input_manager->screen);
                }
                return;
            case SDLK_x:
                if (ctrl && !meta && !shift && !repeat
                        && event->type == SDL_KEYDOWN) {
                    screen_resize_to_fit(input_manager->screen);
                }
                return;
            case SDLK_g:
                if (ctrl && !meta && !shift && !repeat
                        && event->type == SDL_KEYDOWN) {
                    screen_resize_to_pixel_perfect(input_manager->screen);
                }
                return;
            case SDLK_i:
                if (ctrl && !meta && !shift && !repeat
                        && event->type == SDL_KEYDOWN) {
                    switch_fps_counter_state(input_manager->video_buffer);
                }
                return;
            case SDLK_n:
                if (control && ctrl && !meta
                        && !repeat && event->type == SDL_KEYDOWN) {
                    if (shift) {
                        collapse_notification_panel(input_manager->controller);
                    } else {
                        expand_notification_panel(input_manager->controller);
                    }
                }
                return;
        }

        return;
    }

    if (!control) {
        return;
    }

    struct control_event control_event;
    if (input_key_from_sdl_to_android(event, &control_event)) {
        if (!controller_push_event(input_manager->controller, &control_event)) {
            LOGW("Cannot send control event");
        }
    }
}

void
input_manager_process_mouse_motion(struct input_manager *input_manager,
                                   const SDL_MouseMotionEvent *event) {
    if (!event->state) {
        // do not send motion events when no button is pressed
        return;
    }
    struct control_event control_event;
    if (mouse_motion_from_sdl_to_android(event,
                                         input_manager->screen->frame_size,
                                         &control_event)) {
        if (!controller_push_event(input_manager->controller, &control_event)) {
            LOGW("Cannot send mouse motion event");
        }
    }
}

static bool
is_outside_device_screen(struct input_manager *input_manager, int x, int y)
{
    return x < 0 || x >= input_manager->screen->frame_size.width ||
           y < 0 || y >= input_manager->screen->frame_size.height;
}

void
input_manager_process_mouse_button(struct input_manager *input_manager,
                                   const SDL_MouseButtonEvent *event,
                                   bool control) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (control && event->button == SDL_BUTTON_RIGHT) {
            press_back_or_turn_screen_on(input_manager->controller);
            return;
        }
        if (control && event->button == SDL_BUTTON_MIDDLE) {
            action_home(input_manager->controller, ACTION_DOWN | ACTION_UP);
            return;
        }
        // double-click on black borders resize to fit the device screen
        if (event->button == SDL_BUTTON_LEFT && event->clicks == 2) {
            bool outside = is_outside_device_screen(input_manager,
                                                        event->x,
                                                        event->y);
            if (outside) {
                screen_resize_to_fit(input_manager->screen);
                return;
            }
        }
        // otherwise, send the click event to the device
    }

    if (!control) {
        return;
    }

    struct control_event control_event;
    if (mouse_button_from_sdl_to_android(event,
                                         input_manager->screen->frame_size,
                                         &control_event)) {
        if (!controller_push_event(input_manager->controller, &control_event)) {
            LOGW("Cannot send mouse button event");
        }
    }
}

void
input_manager_process_mouse_wheel(struct input_manager *input_manager,
                                  const SDL_MouseWheelEvent *event) {
    struct position position = {
        .screen_size = input_manager->screen->frame_size,
        .point = get_mouse_point(input_manager->screen),
    };
    struct control_event control_event;
    if (mouse_wheel_from_sdl_to_android(event, position, &control_event)) {
        if (!controller_push_event(input_manager->controller, &control_event)) {
            LOGW("Cannot send mouse wheel event");
        }
    }
}
