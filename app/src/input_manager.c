#include "input_manager.h"

#include <assert.h>
#include <SDL2/SDL_keycode.h>

#include "input_events.h"
#include "screen.h"
#include "util/log.h"

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
is_shortcut_mod(struct sc_input_manager *im, uint16_t sdl_mod) {
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
sc_input_manager_init(struct sc_input_manager *im,
                      const struct sc_input_manager_params *params) {
    // A key/mouse processor may not be present if there is no controller
    assert((!params->kp && !params->mp) || params->controller);
    // A processor must have ops initialized
    assert(!params->kp || params->kp->ops);
    assert(!params->mp || params->mp->ops);

    im->controller = params->controller;
    im->fp = params->fp;
    im->screen = params->screen;
    im->kp = params->kp;
    im->mp = params->mp;

    im->forward_all_clicks = params->forward_all_clicks;
    im->legacy_paste = params->legacy_paste;
    im->clipboard_autosync = params->clipboard_autosync;

    const struct sc_shortcut_mods *shortcut_mods = params->shortcut_mods;
    assert(shortcut_mods->count);
    assert(shortcut_mods->count < SC_MAX_SHORTCUT_MODS);
    for (unsigned i = 0; i < shortcut_mods->count; ++i) {
        uint16_t sdl_mod = to_sdl_mod(shortcut_mods->data[i]);
        assert(sdl_mod);
        im->sdl_shortcut_mods.data[i] = sdl_mod;
    }
    im->sdl_shortcut_mods.count = shortcut_mods->count;

    im->vfinger_down = false;
    im->vfinger_invert_x = false;
    im->vfinger_invert_y = false;

    im->last_keycode = SDLK_UNKNOWN;
    im->last_mod = 0;
    im->key_repeat = 0;

    im->next_sequence = 1; // 0 is reserved for SC_SEQUENCE_INVALID
}

static void
send_keycode(struct sc_input_manager *im, enum android_keycode keycode,
             enum sc_action action, const char *name) {
    assert(im->controller && im->kp);

    // send DOWN event
    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE;
    msg.inject_keycode.action = action == SC_ACTION_DOWN
                              ? AKEY_EVENT_ACTION_DOWN
                              : AKEY_EVENT_ACTION_UP;
    msg.inject_keycode.keycode = keycode;
    msg.inject_keycode.metastate = 0;
    msg.inject_keycode.repeat = 0;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'inject %s'", name);
    }
}

static inline void
action_home(struct sc_input_manager *im, enum sc_action action) {
    send_keycode(im, AKEYCODE_HOME, action, "HOME");
}

static inline void
action_back(struct sc_input_manager *im, enum sc_action action) {
    send_keycode(im, AKEYCODE_BACK, action, "BACK");
}

static inline void
action_app_switch(struct sc_input_manager *im, enum sc_action action) {
    send_keycode(im, AKEYCODE_APP_SWITCH, action, "APP_SWITCH");
}

static inline void
action_power(struct sc_input_manager *im, enum sc_action action) {
    send_keycode(im, AKEYCODE_POWER, action, "POWER");
}

static inline void
action_volume_up(struct sc_input_manager *im, enum sc_action action) {
    send_keycode(im, AKEYCODE_VOLUME_UP, action, "VOLUME_UP");
}

static inline void
action_volume_down(struct sc_input_manager *im, enum sc_action action) {
    send_keycode(im, AKEYCODE_VOLUME_DOWN, action, "VOLUME_DOWN");
}

static inline void
action_menu(struct sc_input_manager *im, enum sc_action action) {
    send_keycode(im, AKEYCODE_MENU, action, "MENU");
}

// turn the screen on if it was off, press BACK otherwise
// If the screen is off, it is turned on only on ACTION_DOWN
static void
press_back_or_turn_screen_on(struct sc_input_manager *im,
                             enum sc_action action) {
    assert(im->controller && im->kp);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON;
    msg.back_or_screen_on.action = action == SC_ACTION_DOWN
                                 ? AKEY_EVENT_ACTION_DOWN
                                 : AKEY_EVENT_ACTION_UP;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'press back or turn screen on'");
    }
}

static void
expand_notification_panel(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'expand notification panel'");
    }
}

static void
expand_settings_panel(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'expand settings panel'");
    }
}

static void
collapse_panels(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'collapse notification panel'");
    }
}

static bool
get_device_clipboard(struct sc_input_manager *im, enum sc_copy_key copy_key) {
    assert(im->controller && im->kp);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_GET_CLIPBOARD;
    msg.get_clipboard.copy_key = copy_key;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'get device clipboard'");
        return false;
    }

    return true;
}

static bool
set_device_clipboard(struct sc_input_manager *im, bool paste,
                     uint64_t sequence) {
    assert(im->controller && im->kp);

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

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_SET_CLIPBOARD;
    msg.set_clipboard.sequence = sequence;
    msg.set_clipboard.text = text_dup;
    msg.set_clipboard.paste = paste;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        free(text_dup);
        LOGW("Could not request 'set device clipboard'");
        return false;
    }

    return true;
}

static void
set_screen_power_mode(struct sc_input_manager *im,
                      enum sc_screen_power_mode mode) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE;
    msg.set_screen_power_mode.mode = mode;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'set screen power mode'");
    }
}

static void
switch_fps_counter_state(struct sc_input_manager *im) {
    struct sc_fps_counter *fps_counter = &im->screen->fps_counter;

    // the started state can only be written from the current thread, so there
    // is no ToCToU issue
    if (sc_fps_counter_is_started(fps_counter)) {
        sc_fps_counter_stop(fps_counter);
    } else {
        sc_fps_counter_start(fps_counter);
        // Any error is already logged
    }
}

static void
clipboard_paste(struct sc_input_manager *im) {
    assert(im->controller && im->kp);

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

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TEXT;
    msg.inject_text.text = text_dup;
    if (!sc_controller_push_msg(im->controller, &msg)) {
        free(text_dup);
        LOGW("Could not request 'paste clipboard'");
    }
}

static void
rotate_device(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_ROTATE_DEVICE;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request device rotation");
    }
}

static void
open_hard_keyboard_settings(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_OPEN_HARD_KEYBOARD_SETTINGS;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request opening hard keyboard settings");
    }
}

static void
apply_orientation_transform(struct sc_input_manager *im,
                            enum sc_orientation transform) {
    struct sc_screen *screen = im->screen;
    enum sc_orientation new_orientation =
        sc_orientation_apply(screen->orientation, transform);
    sc_screen_set_orientation(screen, new_orientation);
}

static void
sc_input_manager_process_text_input(struct sc_input_manager *im,
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
simulate_virtual_finger(struct sc_input_manager *im,
                        enum android_motionevent_action action,
                        struct sc_point point) {
    bool up = action == AMOTION_EVENT_ACTION_UP;

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT;
    msg.inject_touch_event.action = action;
    msg.inject_touch_event.position.screen_size = im->screen->frame_size;
    msg.inject_touch_event.position.point = point;
    msg.inject_touch_event.pointer_id =
        im->forward_all_clicks ? POINTER_ID_VIRTUAL_MOUSE
                               : POINTER_ID_VIRTUAL_FINGER;
    msg.inject_touch_event.pressure = up ? 0.0f : 1.0f;
    msg.inject_touch_event.action_button = 0;
    msg.inject_touch_event.buttons = 0;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request 'inject virtual finger event'");
        return false;
    }

    return true;
}

static struct sc_point
inverse_point(struct sc_point point, struct sc_size size,
              bool invert_x, bool invert_y) {
    if (invert_x) {
        point.x = size.width - point.x;
    }
    if (invert_y) {
        point.y = size.height - point.y;
    }
    return point;
}

static void
sc_input_manager_process_key(struct sc_input_manager *im,
                             const SDL_KeyboardEvent *event) {
    // controller is NULL if --no-control is requested
    bool control = im->controller;

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
                if (im->kp && !shift && !repeat) {
                    action_home(im, action);
                }
                return;
            case SDLK_b: // fall-through
            case SDLK_BACKSPACE:
                if (im->kp && !shift && !repeat) {
                    action_back(im, action);
                }
                return;
            case SDLK_s:
                if (im->kp && !shift && !repeat) {
                    action_app_switch(im, action);
                }
                return;
            case SDLK_m:
                if (im->kp && !shift && !repeat) {
                    action_menu(im, action);
                }
                return;
            case SDLK_p:
                if (im->kp && !shift && !repeat) {
                    action_power(im, action);
                }
                return;
            case SDLK_o:
                if (control && !repeat && down) {
                    enum sc_screen_power_mode mode = shift
                                                   ? SC_SCREEN_POWER_MODE_NORMAL
                                                   : SC_SCREEN_POWER_MODE_OFF;
                    set_screen_power_mode(im, mode);
                }
                return;
            case SDLK_DOWN:
                if (shift) {
                    if (!repeat & down) {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_FLIP_180);
                    }
                } else if (im->kp) {
                    // forward repeated events
                    action_volume_down(im, action);
                }
                return;
            case SDLK_UP:
                if (shift) {
                    if (!repeat & down) {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_FLIP_180);
                    }
                } else if (im->kp) {
                    // forward repeated events
                    action_volume_up(im, action);
                }
                return;
            case SDLK_LEFT:
                if (!repeat && down) {
                    if (shift) {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_FLIP_0);
                    } else {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_270);
                    }
                }
                return;
            case SDLK_RIGHT:
                if (!repeat && down) {
                    if (shift) {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_FLIP_0);
                    } else {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_90);
                    }
                }
                return;
            case SDLK_c:
                if (im->kp && !shift && !repeat && down) {
                    get_device_clipboard(im, SC_COPY_KEY_COPY);
                }
                return;
            case SDLK_x:
                if (im->kp && !shift && !repeat && down) {
                    get_device_clipboard(im, SC_COPY_KEY_CUT);
                }
                return;
            case SDLK_v:
                if (im->kp && !repeat && down) {
                    if (shift || im->legacy_paste) {
                        // inject the text as input events
                        clipboard_paste(im);
                    } else {
                        // store the text in the device clipboard and paste,
                        // without requesting an acknowledgment
                        set_device_clipboard(im, true, SC_SEQUENCE_INVALID);
                    }
                }
                return;
            case SDLK_f:
                if (!shift && !repeat && down) {
                    sc_screen_switch_fullscreen(im->screen);
                }
                return;
            case SDLK_w:
                if (!shift && !repeat && down) {
                    sc_screen_resize_to_fit(im->screen);
                }
                return;
            case SDLK_g:
                if (!shift && !repeat && down) {
                    sc_screen_resize_to_pixel_perfect(im->screen);
                }
                return;
            case SDLK_i:
                if (!shift && !repeat && down) {
                    switch_fps_counter_state(im);
                }
                return;
            case SDLK_n:
                if (control && !repeat && down) {
                    if (shift) {
                        collapse_panels(im);
                    } else if (im->key_repeat == 0) {
                        expand_notification_panel(im);
                    } else {
                        expand_settings_panel(im);
                    }
                }
                return;
            case SDLK_r:
                if (control && !shift && !repeat && down) {
                    rotate_device(im);
                }
                return;
            case SDLK_k:
                if (control && !shift && !repeat && down
                        && im->kp && im->kp->hid) {
                    // Only if the current keyboard is hid
                    open_hard_keyboard_settings(im);
                }
                return;
        }

        return;
    }

    if (!im->kp) {
        return;
    }

    uint64_t ack_to_wait = SC_SEQUENCE_INVALID;
    bool is_ctrl_v = ctrl && !shift && keycode == SDLK_v && down && !repeat;
    if (im->clipboard_autosync && is_ctrl_v) {
        if (im->legacy_paste) {
            // inject the text as input events
            clipboard_paste(im);
            return;
        }

        // Request an acknowledgement only if necessary
        uint64_t sequence = im->kp->async_paste ? im->next_sequence
                                                : SC_SEQUENCE_INVALID;

        // Synchronize the computer clipboard to the device clipboard before
        // sending Ctrl+v, to allow seamless copy-paste.
        bool ok = set_device_clipboard(im, false, sequence);
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
sc_input_manager_process_mouse_motion(struct sc_input_manager *im,
                                      const SDL_MouseMotionEvent *event) {

    if (event->which == SDL_TOUCH_MOUSEID) {
        // simulated from touch events, so it's a duplicate
        return;
    }

    struct sc_mouse_motion_event evt = {
        .position = {
            .screen_size = im->screen->frame_size,
            .point = sc_screen_convert_window_to_frame_coords(im->screen,
                                                              event->x,
                                                              event->y),
        },
        .pointer_id = im->forward_all_clicks ? POINTER_ID_MOUSE
                                             : POINTER_ID_GENERIC_FINGER,
        .xrel = event->xrel,
        .yrel = event->yrel,
        .buttons_state =
            sc_mouse_buttons_state_from_sdl(event->state,
                                            im->forward_all_clicks),
    };

    assert(im->mp->ops->process_mouse_motion);
    im->mp->ops->process_mouse_motion(im->mp, &evt);

    // vfinger must never be used in relative mode
    assert(!im->mp->relative_mode || !im->vfinger_down);

    if (im->vfinger_down) {
        assert(!im->mp->relative_mode); // assert one more time
        struct sc_point mouse =
           sc_screen_convert_window_to_frame_coords(im->screen, event->x,
                                                    event->y);
        struct sc_point vfinger = inverse_point(mouse, im->screen->frame_size,
                                                im->vfinger_invert_x,
                                                im->vfinger_invert_y);
        simulate_virtual_finger(im, AMOTION_EVENT_ACTION_MOVE, vfinger);
    }
}

static void
sc_input_manager_process_touch(struct sc_input_manager *im,
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
            .point =
                sc_screen_convert_drawable_to_frame_coords(im->screen, x, y),
        },
        .action = sc_touch_action_from_sdl(event->type),
        .pointer_id = event->fingerId,
        .pressure = event->pressure,
    };

    im->mp->ops->process_touch(im->mp, &evt);
}

static void
sc_input_manager_process_mouse_button(struct sc_input_manager *im,
                                      const SDL_MouseButtonEvent *event) {
    bool control = im->controller;

    if (event->which == SDL_TOUCH_MOUSEID) {
        // simulated from touch events, so it's a duplicate
        return;
    }

    bool down = event->type == SDL_MOUSEBUTTONDOWN;
    if (!im->forward_all_clicks) {
        if (control) {
            enum sc_action action = down ? SC_ACTION_DOWN : SC_ACTION_UP;

            if (im->kp && event->button == SDL_BUTTON_X1) {
                action_app_switch(im, action);
                return;
            }
            if (event->button == SDL_BUTTON_X2 && down) {
                if (event->clicks < 2) {
                    expand_notification_panel(im);
                } else {
                    expand_settings_panel(im);
                }
                return;
            }
            if (im->kp && event->button == SDL_BUTTON_RIGHT) {
                press_back_or_turn_screen_on(im, action);
                return;
            }
            if (im->kp && event->button == SDL_BUTTON_MIDDLE) {
                action_home(im, action);
                return;
            }
        }

        // double-click on black borders resize to fit the device screen
        if (event->button == SDL_BUTTON_LEFT && event->clicks == 2) {
            int32_t x = event->x;
            int32_t y = event->y;
            sc_screen_hidpi_scale_coords(im->screen, &x, &y);
            SDL_Rect *r = &im->screen->rect;
            bool outside = x < r->x || x >= r->x + r->w
                        || y < r->y || y >= r->y + r->h;
            if (outside) {
                if (down) {
                    sc_screen_resize_to_fit(im->screen);
                }
                return;
            }
        }
        // otherwise, send the click event to the device
    }

    if (!im->mp) {
        return;
    }

    uint32_t sdl_buttons_state = SDL_GetMouseState(NULL, NULL);

    struct sc_mouse_click_event evt = {
        .position = {
            .screen_size = im->screen->frame_size,
            .point = sc_screen_convert_window_to_frame_coords(im->screen,
                                                              event->x,
                                                              event->y),
        },
        .action = sc_action_from_sdl_mousebutton_type(event->type),
        .button = sc_mouse_button_from_sdl(event->button),
        .pointer_id = im->forward_all_clicks ? POINTER_ID_MOUSE
                                             : POINTER_ID_GENERIC_FINGER,
        .buttons_state =
            sc_mouse_buttons_state_from_sdl(sdl_buttons_state,
                                            im->forward_all_clicks),
    };

    assert(im->mp->ops->process_mouse_click);
    im->mp->ops->process_mouse_click(im->mp, &evt);

    if (im->mp->relative_mode) {
        assert(!im->vfinger_down); // vfinger must not be used in relative mode
        // No pinch-to-zoom simulation
        return;
    }

    // Pinch-to-zoom, rotate and tilt simulation.
    //
    // If Ctrl is hold when the left-click button is pressed, then
    // pinch-to-zoom mode is enabled: on every mouse event until the left-click
    // button is released, an additional "virtual finger" event is generated,
    // having a position inverted through the center of the screen.
    //
    // In other words, the center of the rotation/scaling is the center of the
    // screen.
    //
    // To simulate a tilt gesture (a vertical slide with two fingers), Shift
    // can be used instead of Ctrl. The "virtual finger" has a position
    // inverted with respect to the vertical axis of symmetry in the middle of
    // the screen.
    const SDL_Keymod keymod = SDL_GetModState();
    const bool ctrl_pressed = keymod & KMOD_CTRL;
    const bool shift_pressed = keymod & KMOD_SHIFT;
    if (event->button == SDL_BUTTON_LEFT &&
            ((down && !im->vfinger_down &&
              ((ctrl_pressed && !shift_pressed) ||
               (!ctrl_pressed && shift_pressed))) ||
             (!down && im->vfinger_down))) {
        struct sc_point mouse =
            sc_screen_convert_window_to_frame_coords(im->screen, event->x,
                                                                 event->y);
        if (down) {
            im->vfinger_invert_x = ctrl_pressed || shift_pressed;
            im->vfinger_invert_y = ctrl_pressed;
        }
        struct sc_point vfinger = inverse_point(mouse, im->screen->frame_size,
                                                im->vfinger_invert_x,
                                                im->vfinger_invert_y);
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
sc_input_manager_process_mouse_wheel(struct sc_input_manager *im,
                                     const SDL_MouseWheelEvent *event) {
    if (!im->mp->ops->process_mouse_scroll) {
        // The mouse processor does not support scroll events
        return;
    }

    // mouse_x and mouse_y are expressed in pixels relative to the window
    int mouse_x;
    int mouse_y;
    uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);

    struct sc_mouse_scroll_event evt = {
        .position = {
            .screen_size = im->screen->frame_size,
            .point = sc_screen_convert_window_to_frame_coords(im->screen,
                                                              mouse_x, mouse_y),
        },
#if SDL_VERSION_ATLEAST(2, 0, 18)
        .hscroll = CLAMP(event->preciseX, -1.0f, 1.0f),
        .vscroll = CLAMP(event->preciseY, -1.0f, 1.0f),
#else
        .hscroll = CLAMP(event->x, -1, 1),
        .vscroll = CLAMP(event->y, -1, 1),
#endif
        .buttons_state =
            sc_mouse_buttons_state_from_sdl(buttons, im->forward_all_clicks),
    };

    im->mp->ops->process_mouse_scroll(im->mp, &evt);
}

static bool
is_apk(const char *file) {
    const char *ext = strrchr(file, '.');
    return ext && !strcmp(ext, ".apk");
}

static void
sc_input_manager_process_file(struct sc_input_manager *im,
                              const SDL_DropEvent *event) {
    char *file = strdup(event->file);
    SDL_free(event->file);
    if (!file) {
        LOG_OOM();
        return;
    }

    enum sc_file_pusher_action action;
    if (is_apk(file)) {
        action = SC_FILE_PUSHER_ACTION_INSTALL_APK;
    } else {
        action = SC_FILE_PUSHER_ACTION_PUSH_FILE;
    }
    bool ok = sc_file_pusher_request(im->fp, action, file);
    if (!ok) {
        free(file);
    }
}

void
sc_input_manager_handle_event(struct sc_input_manager *im,
                              const SDL_Event *event) {
    bool control = im->controller;
    switch (event->type) {
        case SDL_TEXTINPUT:
            if (!im->kp) {
                break;
            }
            sc_input_manager_process_text_input(im, &event->text);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            // some key events do not interact with the device, so process the
            // event even if control is disabled
            sc_input_manager_process_key(im, &event->key);
            break;
        case SDL_MOUSEMOTION:
            if (!im->mp) {
                break;
            }
            sc_input_manager_process_mouse_motion(im, &event->motion);
            break;
        case SDL_MOUSEWHEEL:
            if (!im->mp) {
                break;
            }
            sc_input_manager_process_mouse_wheel(im, &event->wheel);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // some mouse events do not interact with the device, so process
            // the event even if control is disabled
            sc_input_manager_process_mouse_button(im, &event->button);
            break;
        case SDL_FINGERMOTION:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
            if (!im->mp) {
                break;
            }
            sc_input_manager_process_touch(im, &event->tfinger);
            break;
        case SDL_DROPFILE: {
            if (!control) {
                break;
            }
            sc_input_manager_process_file(im, &event->drop);
        }
    }
}
