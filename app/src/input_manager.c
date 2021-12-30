#include "input_manager.h"

#include <assert.h>
#include <SDL2/SDL_keycode.h>

#include "input_events.h"
#include "util/log.h"

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
sc_mouse_buttons_state_from_sdl(uint32_t buttons_state,
                                bool forward_all_clicks) {
    assert(buttons_state < 0x100); // fits in uint8_t

    uint8_t mask = SC_MOUSE_BUTTON_LEFT;
    if (forward_all_clicks) {
        mask |= SC_MOUSE_BUTTON_RIGHT
              | SC_MOUSE_BUTTON_MIDDLE
              | SC_MOUSE_BUTTON_X1
              | SC_MOUSE_BUTTON_X2;
    }

    return buttons_state & mask;
}

#define SC_SDL_SHORTCUT_MODS_MASK (KMOD_CTRL | KMOD_ALT | KMOD_GUI)

static inline uint16_t
to_sdl_mod(unsigned shortcut_mod) {
    uint16_t sdl_mod = 0;
    if (shortcut_mod & SC_SHORTCUT_MOD_LCTRL) {
        sdl_mod |= KMOD_LCTRL;
    }
    if (shortcut_mod & SC_SHORTCUT_MOD_RCTRL) {
        sdl_mod |= KMOD_RCTRL;
    }
    if (shortcut_mod & SC_SHORTCUT_MOD_LALT) {
        sdl_mod |= KMOD_LALT;
    }
    if (shortcut_mod & SC_SHORTCUT_MOD_RALT) {
        sdl_mod |= KMOD_RALT;
    }
    if (shortcut_mod & SC_SHORTCUT_MOD_LSUPER) {
        sdl_mod |= KMOD_LGUI;
    }
    if (shortcut_mod & SC_SHORTCUT_MOD_RSUPER) {
        sdl_mod |= KMOD_RGUI;
    }
    return sdl_mod;
}

static bool
is_shortcut_mod(struct input_manager *im, uint16_t sdl_mod) {
    // keep only the relevant modifier keys
    sdl_mod &= SC_SDL_SHORTCUT_MODS_MASK;

    assert(im->sdl_shortcut_mods.count);
    assert(im->sdl_shortcut_mods.count < SC_MAX_SHORTCUT_MODS);
    for (unsigned i = 0; i < im->sdl_shortcut_mods.count; ++i) {
        if (im->sdl_shortcut_mods.data[i] == sdl_mod) {
            return true;
        }
    }

    return false;
}

void
input_manager_init(struct input_manager *im, struct controller *controller,
                   struct screen *screen, struct sc_key_processor *kp,
                   struct sc_mouse_processor *mp,
                   const struct scrcpy_options *options) {
    assert(!options->control || (kp && kp->ops));
    assert(!options->control || (mp && mp->ops));

    im->controller = controller;
    im->screen = screen;
    im->kp = kp;
    im->mp = mp;

    im->control = options->control;
    im->forward_all_clicks = options->forward_all_clicks;
    im->legacy_paste = options->legacy_paste;
    im->clipboard_autosync = options->clipboard_autosync;

    const struct sc_shortcut_mods *shortcut_mods = &options->shortcut_mods;
    assert(shortcut_mods->count);
    assert(shortcut_mods->count < SC_MAX_SHORTCUT_MODS);
    for (unsigned i = 0; i < shortcut_mods->count; ++i) {
        uint16_t sdl_mod = to_sdl_mod(shortcut_mods->data[i]);
        assert(sdl_mod);
        im->sdl_shortcut_mods.data[i] = sdl_mod;
    }
    im->sdl_shortcut_mods.count = shortcut_mods->count;

    im->vfinger_down = false;

    im->last_keycode = SDLK_UNKNOWN;
    im->last_mod = 0;
    im->key_repeat = 0;

    im->next_sequence = 1; // 0 is reserved for SC_SEQUENCE_INVALID
}

static void
send_keycode(struct controller *controller, enum android_keycode keycode,
             enum sc_action action, const char *name) {
    // send DOWN event
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_KEYCODE;
    msg.inject_keycode.action = action == SC_ACTION_DOWN
                              ? AKEY_EVENT_ACTION_DOWN
                              : AKEY_EVENT_ACTION_UP;
    msg.inject_keycode.keycode = keycode;
    msg.inject_keycode.metastate = 0;
    msg.inject_keycode.repeat = 0;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'inject %s'", name);
    }
}

static inline void
action_home(struct controller *controller, enum sc_action action) {
    send_keycode(controller, AKEYCODE_HOME, action, "HOME");
}

static inline void
action_back(struct controller *controller, enum sc_action action) {
    send_keycode(controller, AKEYCODE_BACK, action, "BACK");
}

static inline void
action_app_switch(struct controller *controller, enum sc_action action) {
    send_keycode(controller, AKEYCODE_APP_SWITCH, action, "APP_SWITCH");
}

static inline void
action_power(struct controller *controller, enum sc_action action) {
    send_keycode(controller, AKEYCODE_POWER, action, "POWER");
}

static inline void
action_volume_up(struct controller *controller, enum sc_action action) {
    send_keycode(controller, AKEYCODE_VOLUME_UP, action, "VOLUME_UP");
}

static inline void
action_volume_down(struct controller *controller, enum sc_action action) {
    send_keycode(controller, AKEYCODE_VOLUME_DOWN, action, "VOLUME_DOWN");
}

static inline void
action_menu(struct controller *controller, enum sc_action action) {
    send_keycode(controller, AKEYCODE_MENU, action, "MENU");
}

// turn the screen on if it was off, press BACK otherwise
// If the screen is off, it is turned on only on ACTION_DOWN
static void
press_back_or_turn_screen_on(struct controller *controller,
                             enum sc_action action) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON;
    msg.back_or_screen_on.action = action == SC_ACTION_DOWN
                                 ? AKEY_EVENT_ACTION_DOWN
                                 : AKEY_EVENT_ACTION_UP;

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
expand_settings_panel(struct controller *controller) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'expand settings panel'");
    }
}

static void
collapse_panels(struct controller *controller) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_COLLAPSE_PANELS;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'collapse notification panel'");
    }
}

static bool
get_device_clipboard(struct controller *controller,
                     enum get_clipboard_copy_key copy_key) {
    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_GET_CLIPBOARD;
    msg.get_clipboard.copy_key = copy_key;

    if (!controller_push_msg(controller, &msg)) {
        LOGW("Could not request 'get device clipboard'");
        return false;
    }

    return true;
}

static bool
set_device_clipboard(struct controller *controller, bool paste,
                     uint64_t sequence) {
    char *text = SDL_GetClipboardText();
    if (!text) {
        LOGW("Could not get clipboard text: %s", SDL_GetError());
        return false;
    }

    char *text_dup = strdup(text);
    SDL_free(text);
    if (!text_dup) {
        LOGW("Could not strdup input text");
        return false;
    }

    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_SET_CLIPBOARD;
    msg.set_clipboard.sequence = sequence;
    msg.set_clipboard.text = text_dup;
    msg.set_clipboard.paste = paste;

    if (!controller_push_msg(controller, &msg)) {
        free(text_dup);
        LOGW("Could not request 'set device clipboard'");
        return false;
    }

    return true;
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

    char *text_dup = strdup(text);
    SDL_free(text);
    if (!text_dup) {
        LOGW("Could not strdup input text");
        return;
    }

    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = text_dup;
    if (!controller_push_msg(controller, &msg)) {
        free(text_dup);
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

static void
input_manager_process_text_input(struct input_manager *im,
                                 const SDL_TextInputEvent *event) {
    if (!im->kp->ops->process_text) {
        // The key processor does not support text input
        return;
    }

    if (is_shortcut_mod(im, SDL_GetModState())) {
        // A shortcut must never generate text events
        return;
    }

    struct sc_text_event evt = {
        .text = event->text,
    };

    im->kp->ops->process_text(im->kp, &evt);
}

static bool
simulate_virtual_finger(struct input_manager *im,
                        enum android_motionevent_action action,
                        struct sc_point point) {
    bool up = action == AMOTION_EVENT_ACTION_UP;

    struct control_msg msg;
    msg.type = CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    msg.inject_touch_event.action = action;
    msg.inject_touch_event.position.screen_size = im->screen->frame_size;
    msg.inject_touch_event.position.point = point;
    msg.inject_touch_event.pointer_id = POINTER_ID_VIRTUAL_FINGER;
    msg.inject_touch_event.pressure = up ? 0.0f : 1.0f;
    msg.inject_touch_event.buttons = 0;

    if (!controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'inject virtual finger event'");
        return false;
    }

    return true;
}

static struct sc_point
inverse_point(struct sc_point point, struct sc_size size) {
    point.x = size.width - point.x;
    point.y = size.height - point.y;
    return point;
}

static void
input_manager_process_key(struct input_manager *im,
                          const SDL_KeyboardEvent *event) {
    // control: indicates the state of the command-line option --no-control
    bool control = im->control;

    struct controller *controller = im->controller;

    SDL_Keycode keycode = event->keysym.sym;
    uint16_t mod = event->keysym.mod;
    bool down = event->type == SDL_KEYDOWN;
    bool ctrl = event->keysym.mod & KMOD_CTRL;
    bool shift = event->keysym.mod & KMOD_SHIFT;
    bool repeat = event->repeat;

    bool smod = is_shortcut_mod(im, mod);

    if (down && !repeat) {
        if (keycode == im->last_keycode && mod == im->last_mod) {
            ++im->key_repeat;
        } else {
            im->key_repeat = 0;
            im->last_keycode = keycode;
            im->last_mod = mod;
        }
    }

    // The shortcut modifier is pressed
    if (smod) {
        enum sc_action action = down ? SC_ACTION_DOWN : SC_ACTION_UP;
        switch (keycode) {
            case SDLK_h:
                if (control && !shift && !repeat) {
                    action_home(controller, action);
                }
                return;
            case SDLK_b: // fall-through
            case SDLK_BACKSPACE:
                if (control && !shift && !repeat) {
                    action_back(controller, action);
                }
                return;
            case SDLK_s:
                if (control && !shift && !repeat) {
                    action_app_switch(controller, action);
                }
                return;
            case SDLK_m:
                if (control && !shift && !repeat) {
                    action_menu(controller, action);
                }
                return;
            case SDLK_p:
                if (control && !shift && !repeat) {
                    action_power(controller, action);
                }
                return;
            case SDLK_o:
                if (control && !repeat && down) {
                    enum screen_power_mode mode = shift
                                                ? SCREEN_POWER_MODE_NORMAL
                                                : SCREEN_POWER_MODE_OFF;
                    set_screen_power_mode(controller, mode);
                }
                return;
            case SDLK_DOWN:
                if (control && !shift) {
                    // forward repeated events
                    action_volume_down(controller, action);
                }
                return;
            case SDLK_UP:
                if (control && !shift) {
                    // forward repeated events
                    action_volume_up(controller, action);
                }
                return;
            case SDLK_LEFT:
                if (!shift && !repeat && down) {
                    rotate_client_left(im->screen);
                }
                return;
            case SDLK_RIGHT:
                if (!shift && !repeat && down) {
                    rotate_client_right(im->screen);
                }
                return;
            case SDLK_c:
                if (control && !shift && !repeat && down) {
                    get_device_clipboard(controller,
                                         GET_CLIPBOARD_COPY_KEY_COPY);
                }
                return;
            case SDLK_x:
                if (control && !shift && !repeat && down) {
                    get_device_clipboard(controller,
                                         GET_CLIPBOARD_COPY_KEY_CUT);
                }
                return;
            case SDLK_v:
                if (control && !repeat && down) {
                    if (shift || im->legacy_paste) {
                        // inject the text as input events
                        clipboard_paste(controller);
                    } else {
                        // store the text in the device clipboard and paste,
                        // without requesting an acknowledgment
                        set_device_clipboard(controller, true,
                                             SC_SEQUENCE_INVALID);
                    }
                }
                return;
            case SDLK_f:
                if (!shift && !repeat && down) {
                    screen_switch_fullscreen(im->screen);
                }
                return;
            case SDLK_w:
                if (!shift && !repeat && down) {
                    screen_resize_to_fit(im->screen);
                }
                return;
            case SDLK_g:
                if (!shift && !repeat && down) {
                    screen_resize_to_pixel_perfect(im->screen);
                }
                return;
            case SDLK_i:
                if (!shift && !repeat && down) {
                    switch_fps_counter_state(&im->screen->fps_counter);
                }
                return;
            case SDLK_n:
                if (control && !repeat && down) {
                    if (shift) {
                        collapse_panels(controller);
                    } else if (im->key_repeat == 0) {
                        expand_notification_panel(controller);
                    } else {
                        expand_settings_panel(controller);
                    }
                }
                return;
            case SDLK_r:
                if (control && !shift && !repeat && down) {
                    rotate_device(controller);
                }
                return;
        }

        return;
    }

    if (!control) {
        return;
    }

    uint64_t ack_to_wait = SC_SEQUENCE_INVALID;
    bool is_ctrl_v = ctrl && !shift && keycode == SDLK_v && down && !repeat;
    if (im->clipboard_autosync && is_ctrl_v) {
        if (im->legacy_paste) {
            // inject the text as input events
            clipboard_paste(controller);
            return;
        }

        // Request an acknowledgement only if necessary
        uint64_t sequence = im->kp->async_paste ? im->next_sequence
                                                : SC_SEQUENCE_INVALID;

        // Synchronize the computer clipboard to the device clipboard before
        // sending Ctrl+v, to allow seamless copy-paste.
        bool ok = set_device_clipboard(controller, false, sequence);
        if (!ok) {
            LOGW("Clipboard could not be synchronized, Ctrl+v not injected");
            return;
        }

        if (im->kp->async_paste) {
            // The key processor must wait for this ack before injecting Ctrl+v
            ack_to_wait = sequence;
            // Increment only when the request succeeded
            ++im->next_sequence;
        }
    }

    struct sc_key_event evt = {
        .action = sc_action_from_sdl_keyboard_type(event->type),
        .keycode = sc_keycode_from_sdl(event->keysym.sym),
        .scancode = sc_scancode_from_sdl(event->keysym.scancode),
        .repeat = event->repeat,
        .mods_state = sc_mods_state_from_sdl(event->keysym.mod),
    };

    assert(im->kp->ops->process_key);
    im->kp->ops->process_key(im->kp, &evt, ack_to_wait);
}

static void
input_manager_process_mouse_motion(struct input_manager *im,
                                   const SDL_MouseMotionEvent *event) {

    uint32_t mask = SDL_BUTTON_LMASK;
    if (im->forward_all_clicks) {
        mask |= SDL_BUTTON_MMASK | SDL_BUTTON_RMASK;
    }
    if (!(event->state & mask)) {
        // do not send motion events when no click is pressed
        return;
    }
    if (event->which == SDL_TOUCH_MOUSEID) {
        // simulated from touch events, so it's a duplicate
        return;
    }

    struct sc_mouse_motion_event evt = {
        .position = {
            .screen_size = im->screen->frame_size,
            .point = screen_convert_window_to_frame_coords(im->screen,
                                                           event->x, event->y),
        },
        .buttons_state =
            sc_mouse_buttons_state_from_sdl(event->state,
                                            im->forward_all_clicks),
    };

    assert(im->mp->ops->process_mouse_motion);
    im->mp->ops->process_mouse_motion(im->mp, &evt);

    if (im->vfinger_down) {
        struct sc_point mouse =
            screen_convert_window_to_frame_coords(im->screen, event->x,
                                                  event->y);
        struct sc_point vfinger = inverse_point(mouse, im->screen->frame_size);
        simulate_virtual_finger(im, AMOTION_EVENT_ACTION_MOVE, vfinger);
    }
}

static void
input_manager_process_touch(struct input_manager *im,
                            const SDL_TouchFingerEvent *event) {
    if (!im->mp->ops->process_touch) {
        // The mouse processor does not support touch events
        return;
    }

    int dw;
    int dh;
    SDL_GL_GetDrawableSize(im->screen->window, &dw, &dh);

    // SDL touch event coordinates are normalized in the range [0; 1]
    int32_t x = event->x * dw;
    int32_t y = event->y * dh;

    struct sc_touch_event evt = {
        .position = {
            .screen_size = im->screen->frame_size,
            .point = screen_convert_drawable_to_frame_coords(im->screen, x, y),
        },
        .action = sc_touch_action_from_sdl(event->type),
        .pointer_id = event->fingerId,
        .pressure = event->pressure,
    };

    im->mp->ops->process_touch(im->mp, &evt);
}

static void
input_manager_process_mouse_button(struct input_manager *im,
                                   const SDL_MouseButtonEvent *event) {
    bool control = im->control;

    if (event->which == SDL_TOUCH_MOUSEID) {
        // simulated from touch events, so it's a duplicate
        return;
    }

    bool down = event->type == SDL_MOUSEBUTTONDOWN;
    if (!im->forward_all_clicks) {
        enum sc_action action = down ? SC_ACTION_DOWN : SC_ACTION_UP;

        if (control && event->button == SDL_BUTTON_X1) {
            action_app_switch(im->controller, action);
            return;
        }
        if (control && event->button == SDL_BUTTON_X2 && down) {
            if (event->clicks < 2) {
                expand_notification_panel(im->controller);
            } else {
                expand_settings_panel(im->controller);
            }
            return;
        }
        if (control && event->button == SDL_BUTTON_RIGHT) {
            press_back_or_turn_screen_on(im->controller, action);
            return;
        }
        if (control && event->button == SDL_BUTTON_MIDDLE) {
            action_home(im->controller, action);
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
                if (down) {
                    screen_resize_to_fit(im->screen);
                }
                return;
            }
        }
        // otherwise, send the click event to the device
    }

    if (!control) {
        return;
    }

    uint32_t sdl_buttons_state = SDL_GetMouseState(NULL, NULL);

    struct sc_mouse_click_event evt = {
        .position = {
            .screen_size = im->screen->frame_size,
            .point = screen_convert_window_to_frame_coords(im->screen, event->x,
                                                           event->y),
        },
        .action = sc_action_from_sdl_mousebutton_type(event->type),
        .button = sc_mouse_button_from_sdl(event->button),
        .buttons_state =
            sc_mouse_buttons_state_from_sdl(sdl_buttons_state,
                                            im->forward_all_clicks),
    };

    assert(im->mp->ops->process_mouse_click);
    im->mp->ops->process_mouse_click(im->mp, &evt);

    // Pinch-to-zoom simulation.
    //
    // If Ctrl is hold when the left-click button is pressed, then
    // pinch-to-zoom mode is enabled: on every mouse event until the left-click
    // button is released, an additional "virtual finger" event is generated,
    // having a position inverted through the center of the screen.
    //
    // In other words, the center of the rotation/scaling is the center of the
    // screen.
#define CTRL_PRESSED (SDL_GetModState() & (KMOD_LCTRL | KMOD_RCTRL))
    if ((down && !im->vfinger_down && CTRL_PRESSED)
            || (!down && im->vfinger_down)) {
        struct sc_point mouse =
            screen_convert_window_to_frame_coords(im->screen, event->x,
                                                              event->y);
        struct sc_point vfinger = inverse_point(mouse, im->screen->frame_size);
        enum android_motionevent_action action = down
                                               ? AMOTION_EVENT_ACTION_DOWN
                                               : AMOTION_EVENT_ACTION_UP;
        if (!simulate_virtual_finger(im, action, vfinger)) {
            return;
        }
        im->vfinger_down = down;
    }
}

static void
input_manager_process_mouse_wheel(struct input_manager *im,
                                  const SDL_MouseWheelEvent *event) {
    if (!im->mp->ops->process_mouse_scroll) {
        // The mouse processor does not support scroll events
        return;
    }

    // mouse_x and mouse_y are expressed in pixels relative to the window
    int mouse_x;
    int mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    struct sc_mouse_scroll_event evt = {
        .position = {
            .screen_size = im->screen->frame_size,
            .point = screen_convert_window_to_frame_coords(im->screen,
                                                           mouse_x, mouse_y),
        },
        .hscroll = event->x,
        .vscroll = event->y,
    };

    im->mp->ops->process_mouse_scroll(im->mp, &evt);
}

bool
input_manager_handle_event(struct input_manager *im, SDL_Event *event) {
    switch (event->type) {
        case SDL_TEXTINPUT:
            if (!im->control) {
                return true;
            }
            input_manager_process_text_input(im, &event->text);
            return true;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            // some key events do not interact with the device, so process the
            // event even if control is disabled
            input_manager_process_key(im, &event->key);
            return true;
        case SDL_MOUSEMOTION:
            if (!im->control) {
                break;
            }
            input_manager_process_mouse_motion(im, &event->motion);
            return true;
        case SDL_MOUSEWHEEL:
            if (!im->control) {
                break;
            }
            input_manager_process_mouse_wheel(im, &event->wheel);
            return true;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // some mouse events do not interact with the device, so process
            // the event even if control is disabled
            input_manager_process_mouse_button(im, &event->button);
            return true;
        case SDL_FINGERMOTION:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
            input_manager_process_touch(im, &event->tfinger);
            return true;
    }

    return false;
}
