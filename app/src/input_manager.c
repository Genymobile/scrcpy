#include "input_manager.h"

#include <SDL2/SDL_assert.h>
#include "event_converter.h"
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
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_KEYCODE;
    msg.inject_keycode.keycode = keycode;
    msg.inject_keycode.metastate = 0;

    if (actions & ACTION_DOWN) {
        msg.inject_keycode.action = AKEY_EVENT_ACTION_DOWN;
        if (!controller_push_msg(controller, &msg)) {
            LOGW("Could not request 'inject %s (DOWN)'", name);
            return;
        }
    }

    if (actions & ACTION_UP) {
        msg.inject_keycode.action = AKEY_EVENT_ACTION_UP;
        if (!controller_push_msg(controller, &msg)) {
            LOGW("Could not request 'inject %s (UP)'", name);
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
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'press back or turn screen on'");
    }
}

static void
expand_notification_panel(struct controller *controller) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'expand notification panel'");
    }
}

static void
collapse_notification_panel(struct controller *controller) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_COLLAPSE_NOTIFICATION_PANEL;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'collapse notification panel'");
    }
}

static void
request_device_clipboard(struct controller *controller) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_GET_CLIPBOARD;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request device clipboard");
    }
}

static void
set_device_clipboard(struct controller *controller) {
    char *text = SDL_GetClipboardText();
    if (!text) {
        LOGW("Could not get clipboard text: %s", SDL_GetError());
        return;
    }
    if (!*text) {
        // empty text
        SDL_free(text);
        return;
    }

    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_SET_CLIPBOARD;
    msg.set_clipboard.text = text;

    if (!controller_push_msg(controller, &msg)) {
        SDL_free(text);
        LOGW("Could not request 'set device clipboard'");
    }
}

static void
set_screen_power_mode(struct controller *controller,
                      enum screen_power_mode mode) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE;
    msg.set_screen_power_mode.mode = mode;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'set screen power mode'");
    }
}

static void
switch_fps_counter_state(struct fps_counter *fps_counter) {
    // the started state can only be written from the current thread, so there
    // is no ToCToU issue
    if (fps_counter_is_started(fps_counter)) {
        fps_counter_stop(fps_counter);
        LOGI("FPS counter stopped");
    } else {
        if (fps_counter_start(fps_counter)) {
            LOGI("FPS counter started");
        } else {
            LOGE("FPS counter starting failed");
        }
    }
}

static void
clipboard_paste(struct controller *controller) {
    char *text = SDL_GetClipboardText();
    if (!text) {
        LOGW("Could not get clipboard text: %s", SDL_GetError());
        return;
    }
    if (!*text) {
        // empty text
        SDL_free(text);
        return;
    }

    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = text;
    if (!controller_push_msg(controller, &msg)) {
        SDL_free(text);
        LOGW("Could not request 'paste clipboard'");
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
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = SDL_strdup(event->text);
    if (!msg.inject_text.text) {
        LOGW("Could not strdup input text");
        return;
    }
    if (!controller_push_msg(input_manager->controller, &msg)) {
        SDL_free(msg.inject_text.text);
        LOGW("Could not request 'inject text'");
    }
}

void
input_manager_process_key(struct input_manager *input_manager,
                          const SDL_KeyboardEvent *event,
                          bool control) {
    // control: indicates the state of the command-line option --no-control
    // ctrl: the Ctrl key

    bool ctrl = event->keysym.mod & (KMOD_LCTRL | KMOD_RCTRL);
    bool alt = event->keysym.mod & (KMOD_LALT | KMOD_RALT);
    bool meta = event->keysym.mod & (KMOD_LGUI | KMOD_RGUI);

    // use Cmd on macOS, Ctrl on other platforms
#ifdef __APPLE__
    bool cmd = !ctrl && meta;
#else
    if (meta) {
        // no shortcuts involve Meta on platforms other than macOS, and it must
        // not be forwarded to the device
        return;
    }
    bool cmd = ctrl; // && !meta, already guaranteed
#endif

    if (alt) {
        // no shortcuts involve Alt, and it must not be forwarded to the device
        return;
    }

    struct controller *controller = input_manager->controller;

    // capture all Ctrl events
    if (ctrl || cmd) {
        SDL_Keycode keycode = event->keysym.sym;
        bool down = event->type == SDL_KEYDOWN;
        int action = down ? ACTION_DOWN : ACTION_UP;
        bool repeat = event->repeat;
        bool shift = event->keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT);
        switch (keycode) {
            case SDLK_h:
                // Ctrl+h on all platform, since Cmd+h is already captured by
                // the system on macOS to hide the window
                if (control && ctrl && !meta && !shift && !repeat) {
                    action_home(controller, action);
                }
                return;
            case SDLK_b: // fall-through
            case SDLK_BACKSPACE:
                if (control && cmd && !shift && !repeat) {
                    action_back(controller, action);
                }
                return;
            case SDLK_s:
                if (control && cmd && !shift && !repeat) {
                    action_app_switch(controller, action);
                }
                return;
            case SDLK_m:
                // Ctrl+m on all platform, since Cmd+m is already captured by
                // the system on macOS to minimize the window
                if (control && ctrl && !meta && !shift && !repeat) {
                    action_menu(controller, action);
                }
                return;
            case SDLK_p:
                if (control && cmd && !shift && !repeat) {
                    action_power(controller, action);
                }
                return;
            case SDLK_o:
                if (control && cmd && !shift && down) {
                    set_screen_power_mode(controller, SCREEN_POWER_MODE_OFF);
                }
                return;
            case SDLK_DOWN:
                if (control && cmd && !shift) {
                    // forward repeated events
                    action_volume_down(controller, action);
                }
                return;
            case SDLK_UP:
                if (control && cmd && !shift) {
                    // forward repeated events
                    action_volume_up(controller, action);
                }
                return;
            case SDLK_c:
                if (control && cmd && !shift && !repeat && down) {
                    request_device_clipboard(controller);
                }
                return;
            case SDLK_v:
                if (control && cmd && !repeat && down) {
                    if (shift) {
                        // store the text in the device clipboard
                        set_device_clipboard(controller);
                    } else {
                        // inject the text as input events
                        clipboard_paste(controller);
                    }
                }
                return;
            case SDLK_f:
                if (!shift && cmd && !repeat && down) {
                    screen_switch_fullscreen(input_manager->screen);
                }
                return;
            case SDLK_x:
                if (!shift && cmd && !repeat && down) {
                    screen_resize_to_fit(input_manager->screen);
                }
                return;
            case SDLK_g:
                if (!shift && cmd && !repeat && down) {
                    screen_resize_to_pixel_perfect(input_manager->screen);
                }
                return;
            case SDLK_i:
                if (!shift && cmd && !repeat && down) {
                    struct fps_counter *fps_counter =
                        input_manager->video_buffer->fps_counter;
                    switch_fps_counter_state(fps_counter);
                }
                return;
            case SDLK_n:
                if (control && cmd && !repeat && down) {
                    if (shift) {
                        collapse_notification_panel(controller);
                    } else {
                        expand_notification_panel(controller);
                    }
                }
                return;
        }

        return;
    }

    if (!control) {
        return;
    }

    struct control_msg msg;
    if (convert_input_key(event, &msg)) {
        if (!controller_push_msg(controller, &msg)) {
            LOGW("Could not request 'inject keycode'");
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
    struct control_msg msg;
    if (convert_mouse_motion(event, input_manager->screen->frame_size, &msg)) {
        if (!controller_push_msg(input_manager->controller, &msg)) {
            LOGW("Could not request 'inject mouse motion event'");
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
            bool outside =
                is_outside_device_screen(input_manager, event->x, event->y);
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

    struct control_msg msg;
    if (convert_mouse_button(event, input_manager->screen->frame_size, &msg)) {
        if (!controller_push_msg(input_manager->controller, &msg)) {
            LOGW("Could not request 'inject mouse button event'");
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
    struct control_msg msg;
    if (convert_mouse_wheel(event, position, &msg)) {
        if (!controller_push_msg(input_manager->controller, &msg)) {
            LOGW("Could not request 'inject mouse wheel event'");
        }
    }
}
