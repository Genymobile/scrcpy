#ifndef SCREEN_H
#define SCREEN_H

#include <SDL2/SDL.h>
#include <libavformat/avformat.h>

#include "common.h"

struct screen {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    struct size frame_size;
    //used only in fullscreen mode to know the windowed window size
    struct size windowed_window_size;
    SDL_bool texture_initialized;
    SDL_bool fullscreen;
};

#define SCREEN_INITIALIZER {          \
    .window = NULL,                   \
    .renderer = NULL,                 \
    .texture = NULL,                  \
    .frame_size = {                   \
        .width = 0,                   \
        .height = 0,                  \
    },                                \
    .windowed_window_size = {         \
        .width = 0,                   \
        .height = 0,                  \
    },                                \
    .texture_initialized = SDL_FALSE, \
    .fullscreen = SDL_FALSE,          \
}

// init SDL and set appropriate hints
SDL_bool sdl_init_and_configure(void);

// initialize default values
void screen_init(struct screen *screen);

// initialize screen, create window, renderer and texture
SDL_bool screen_init_rendering(struct screen *screen,
                               const char *device_name,
                               struct size frame_size);

// destroy window, renderer and texture (if any)
void screen_destroy(struct screen *screen);

// resize if necessary and write the frame into the texture
SDL_bool screen_update(struct screen *screen, const AVFrame *frame);

// render the texture to the renderer
void screen_render(struct screen *screen);

// switch the fullscreen mode
void screen_switch_fullscreen(struct screen *screen);

// resize window to optimal size (remove black borders)
void screen_resize_to_fit(struct screen *screen);

// resize window to 1:1 (pixel-perfect)
void screen_resize_to_pixel_perfect(struct screen *screen);

#endif
