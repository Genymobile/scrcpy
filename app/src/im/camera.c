#include "input_manager.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "android/input.h"
#include "android/keycodes.h"
#include "input_events.h"
#include "screen.h"
#include "shortcut_mod.h"
#include "util/log.h"
#include "camera.h"

static void
camera_toggle_torch(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_CAMERA_TOGGLE_TORCH;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request camera toggle torch");
    }
}

static void
camera_zoom_in(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_CAMERA_ZOOM_IN;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request camera zoom in");
    }
}

static void
camera_zoom_out(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_CAMERA_ZOOM_OUT;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request camera zoom out");
    }
}

static void
reset_video(struct sc_input_manager *im) {
    assert(im->controller);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_RESET_VIDEO;

    if (!sc_controller_push_msg(im->controller, &msg)) {
        LOGW("Could not request reset video");
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
sc_input_manager_process_key_camera(struct sc_input_manager *im,
                                    const SDL_KeyboardEvent *event) {
    bool control = im->controller;
    bool paused = im->screen->paused;
    bool video = im->screen->video;

    SDL_Keycode sdl_keycode = event->keysym.sym;
    uint16_t mod = event->keysym.mod;
    bool down = event->type == SDL_KEYDOWN;
    bool shift = event->keysym.mod & KMOD_SHIFT;
    bool repeat = event->repeat;

    uint16_t mods = im->sdl_shortcut_mods;
    bool is_shortcut = sc_shortcut_mods_is_shortcut_mod(mods, mod)
                    || sc_shortcut_mods_is_shortcut_key(mods, sdl_keycode);

    if (down && !repeat) {
        if (sdl_keycode == im->last_keycode && mod == im->last_mod) {
            ++im->key_repeat;
        } else {
            im->key_repeat = 0;
            im->last_keycode = sdl_keycode;
            im->last_mod = mod;
        }
    }

    if (is_shortcut) {
        switch (sdl_keycode) {
            // Camera
            case SDLK_1:
                if (control && video && !shift && down) {
                    camera_zoom_in(im);
                }
                return;
            case SDLK_2:
                if (control && video && !shift && down) {
                    camera_zoom_out(im);
                }
                return;
            case SDLK_q:
                if (control && video && !shift && !repeat && down) {
                    camera_toggle_torch(im);
                }
                return;
            // Window
            case SDLK_f:
                if (video && !shift && !repeat && down) {
                    sc_screen_toggle_fullscreen(im->screen);
                }
                return;
            case SDLK_w:
                if (video && !shift && !repeat && down) {
                    sc_screen_resize_to_fit(im->screen);
                }
                return;
            case SDLK_g:
                if (video && !shift && !repeat && down) {
                    sc_screen_resize_to_pixel_perfect(im->screen);
                }
                return;
            case SDLK_DOWN:
                if (shift) {
                    if (video && !repeat && down) {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_FLIP_180);
                    }
                }
                return;
            case SDLK_UP:
                if (shift) {
                    if (video && !repeat && down) {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_FLIP_180);
                    }
                }
                return;
            case SDLK_LEFT:
                if (video && !repeat && down) {
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
                if (video && !repeat && down) {
                    if (shift) {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_FLIP_0);
                    } else {
                        apply_orientation_transform(im,
                                                    SC_ORIENTATION_90);
                    }
                }
                return;
            // Video
            case SDLK_r:
                if (control && !repeat && down && !paused) {
                    if (shift) {
                        reset_video(im);
                    }
                }
                return;
        }
    }
}

void
sc_input_manager_handle_event_camera(struct sc_input_manager *im,
                                     const SDL_Event *event) {
    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            sc_input_manager_process_key_camera(im, &event->key);
            break;
    }
}
