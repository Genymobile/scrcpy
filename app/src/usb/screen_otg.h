#ifndef SC_SCREEN_OTG_H
#define SC_SCREEN_OTG_H

#include "common.h"

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "hid_keyboard.h"
#include "hid_mouse.h"

struct sc_screen_otg {
    struct sc_hid_keyboard *keyboard;
    struct sc_hid_mouse *mouse;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    // See equivalent mechanism in screen.h
    SDL_Keycode mouse_capture_key_pressed;
};

struct sc_screen_otg_params {
    struct sc_hid_keyboard *keyboard;
    struct sc_hid_mouse *mouse;

    const char *window_title;
    bool always_on_top;
    int16_t window_x; // accepts SC_WINDOW_POSITION_UNDEFINED
    int16_t window_y; // accepts SC_WINDOW_POSITION_UNDEFINED
    uint16_t window_width;
    uint16_t window_height;
    bool window_borderless;
};

bool
sc_screen_otg_init(struct sc_screen_otg *screen,
                   const struct sc_screen_otg_params *params);

void
sc_screen_otg_destroy(struct sc_screen_otg *screen);

void
sc_screen_otg_handle_event(struct sc_screen_otg *screen, SDL_Event *event);

#endif
