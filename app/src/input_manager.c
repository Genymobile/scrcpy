#include "input_manager.h"

#include <assert.h>

#include "config.h"
#include "event_converter.h"
#include "util/lock.h"
#include "util/log.h"

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
set_device_clipboard(struct controller *controller, bool paste) {
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
    msg.set_clipboard.paste = paste;

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

static void
rotate_device(struct controller *controller) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_ROTATE_DEVICE;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request device rotation");
    }
}

static void
rotate_client_left(struct screen *screen) {
    unsigned new_rotation = (screen->rotation + 1) % 4;
    screen_set_rotation(screen, new_rotation);
}

static void
rotate_client_right(struct screen *screen) {
    unsigned new_rotation = (screen->rotation + 3) % 4;
    screen_set_rotation(screen, new_rotation);
}

void
input_manager_process_text_input(struct input_manager *im,
                                 const SDL_TextInputEvent *event) {
    if (!im->prefer_text) {
        char c = event->text[0];
        if (isalpha(c) || c == ' ') {
            assert(event->text[1] == '\0');
            // letters and space are handled as raw key event
            return;
        }
    }

    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = SDL_strdup(event->text);
    if (!msg.inject_text.text) {
        LOGW("Could not strdup input text");
        return;
    }
    if (!controller_push_msg(im->controller, &msg)) {
        SDL_free(msg.inject_text.text);
        LOGW("Could not request 'inject text'");
    }
}

static bool
convert_input_key(const SDL_KeyboardEvent *from, struct control_msg *to,
                  bool prefer_text) {
    to->type = CONTROL_MSG_TYPE_INJECT_KEYCODE;

    if (!convert_keycode_action(from->type, &to->inject_keycode.action)) {
        return false;
    }

    uint16_t mod = from->keysym.mod;
    if (!convert_keycode(from->keysym.sym, &to->inject_keycode.keycode, mod,
                         prefer_text)) {
        return false;
    }

    to->inject_keycode.metastate = convert_meta_state(mod);

    return true;
}

void
input_manager_process_key(struct input_manager *im,
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

    struct controller *controller = im->controller;

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
                if (control && cmd && down) {
                    enum screen_power_mode mode = shift
                                                ? SCREEN_POWER_MODE_NORMAL
                                                : SCREEN_POWER_MODE_OFF;
                    set_screen_power_mode(controller, mode);
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
            case SDLK_LEFT:
                if (cmd && !shift && down) {
                    rotate_client_left(im->screen);
                }
                return;
            case SDLK_RIGHT:
                if (cmd && !shift && down) {
                    rotate_client_right(im->screen);
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
                        // store the text in the device clipboard and paste
                        set_device_clipboard(controller, true);
                    } else {
                        // inject the text as input events
                        clipboard_paste(controller);
                    }
                }
                return;
            case SDLK_f:
                if (!shift && cmd && !repeat && down) {
                    screen_switch_fullscreen(im->screen);
                }
                return;
            case SDLK_x:
                if (!shift && cmd && !repeat && down) {
                    screen_resize_to_fit(im->screen);
                }
                return;
            case SDLK_g:
                if (!shift && cmd && !repeat && down) {
                    screen_resize_to_pixel_perfect(im->screen);
                }
                return;
            case SDLK_i:
                if (!shift && cmd && !repeat && down) {
                    struct fps_counter *fps_counter =
                        im->video_buffer->fps_counter;
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
            case SDLK_r:
                if (control && cmd && !shift && !repeat && down) {
                    rotate_device(controller);
                }
                return;
        }

        return;
    }

    if (!control) {
        return;
    }

    struct control_msg msg;
    if (convert_input_key(event, &msg, im->prefer_text)) {
        if (!controller_push_msg(controller, &msg)) {
            LOGW("Could not request 'inject keycode'");
        }
    }
}

static bool
convert_mouse_motion(const SDL_MouseMotionEvent *from, struct screen *screen,
                     struct control_msg *to) {
    to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    to->inject_touch_event.action = AMOTION_EVENT_ACTION_MOVE;
    to->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
    to->inject_touch_event.position.screen_size = screen->frame_size;
    to->inject_touch_event.position.point =
        screen_convert_to_frame_coords(screen, from->x, from->y);
    to->inject_touch_event.pressure = 1.f;
    to->inject_touch_event.buttons = convert_mouse_buttons(from->state);

    return true;
}

void
input_manager_process_mouse_motion(struct input_manager *im,
                                   const SDL_MouseMotionEvent *event) {
    if (!event->state) {
        // do not send motion events when no button is pressed
        return;
    }
    if (event->which == SDL_TOUCH_MOUSEID) {
        // simulated from touch events, so it's a duplicate
        return;
    }
    struct control_msg msg;
    if (convert_mouse_motion(event, im->screen, &msg)) {
        if (!controller_push_msg(im->controller, &msg)) {
            LOGW("Could not request 'inject mouse motion event'");
        }
    }
}

static bool
convert_touch(const SDL_TouchFingerEvent *from, struct screen *screen,
              struct control_msg *to) {
    to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

    if (!convert_touch_action(from->type, &to->inject_touch_event.action)) {
        return false;
    }

    to->inject_touch_event.pointer_id = from->fingerId;
    to->inject_touch_event.position.screen_size = screen->frame_size;

    int ww;
    int wh;
    SDL_GL_GetDrawableSize(screen->window, &ww, &wh);

    // SDL touch event coordinates are normalized in the range [0; 1]
    int32_t x = from->x * ww;
    int32_t y = from->y * wh;
    to->inject_touch_event.position.point =
        screen_convert_to_frame_coords(screen, x, y);

    to->inject_touch_event.pressure = from->pressure;
    to->inject_touch_event.buttons = 0;
    return true;
}

void
input_manager_process_touch(struct input_manager *im,
                            const SDL_TouchFingerEvent *event) {
    struct control_msg msg;
    if (convert_touch(event, im->screen, &msg)) {
        if (!controller_push_msg(im->controller, &msg)) {
            LOGW("Could not request 'inject touch event'");
        }
    }
}

static bool
convert_mouse_button(const SDL_MouseButtonEvent *from, struct screen *screen,
                     struct control_msg *to) {
    to->type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;

    if (!convert_mouse_action(from->type, &to->inject_touch_event.action)) {
        return false;
    }

    to->inject_touch_event.pointer_id = POINTER_ID_MOUSE;
    to->inject_touch_event.position.screen_size = screen->frame_size;
    to->inject_touch_event.position.point =
        screen_convert_to_frame_coords(screen, from->x, from->y);
    to->inject_touch_event.pressure = 1.f;
    to->inject_touch_event.buttons =
        convert_mouse_buttons(SDL_BUTTON(from->button));

    return true;
}

void
input_manager_process_mouse_button(struct input_manager *im,
                                   const SDL_MouseButtonEvent *event,
                                   bool control) {
    if (event->which == SDL_TOUCH_MOUSEID) {
        // simulated from touch events, so it's a duplicate
        return;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (control && event->button == SDL_BUTTON_RIGHT) {
            press_back_or_turn_screen_on(im->controller);
            return;
        }
        if (control && event->button == SDL_BUTTON_MIDDLE) {
            action_home(im->controller, ACTION_DOWN | ACTION_UP);
            return;
        }

        // double-click on black borders resize to fit the device screen
        if (event->button == SDL_BUTTON_LEFT && event->clicks == 2) {
            int32_t x = event->x;
            int32_t y = event->y;
            screen_hidpi_scale_coords(im->screen, &x, &y);
            SDL_Rect *r = &im->screen->rect;
            bool outside = x < r->x || x >= r->x + r->w
                        || y < r->y || y >= r->y + r->h;
            if (outside) {
                screen_resize_to_fit(im->screen);
                return;
            }
        }
        // otherwise, send the click event to the device
    }

    if (!control) {
        return;
    }

    struct control_msg msg;
    if (convert_mouse_button(event, im->screen, &msg)) {
        if (!controller_push_msg(im->controller, &msg)) {
            LOGW("Could not request 'inject mouse button event'");
        }
    }
}

static bool
convert_mouse_wheel(const SDL_MouseWheelEvent *from, struct screen *screen,
                    struct control_msg *to) {

    // mouse_x and mouse_y are expressed in pixels relative to the window
    int mouse_x;
    int mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    struct position position = {
        .screen_size = screen->frame_size,
        .point = screen_convert_to_frame_coords(screen, mouse_x, mouse_y),
    };

    to->type = CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT;

    to->inject_scroll_event.position = position;
    to->inject_scroll_event.hscroll = from->x;
    to->inject_scroll_event.vscroll = from->y;

    return true;
}

void
input_manager_process_mouse_wheel(struct input_manager *im,
                                  const SDL_MouseWheelEvent *event) {
    struct control_msg msg;
    if (convert_mouse_wheel(event, im->screen, &msg)) {
        if (!controller_push_msg(im->controller, &msg)) {
            LOGW("Could not request 'inject mouse wheel event'");
        }
    }
}
