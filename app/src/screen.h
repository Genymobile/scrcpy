#ifndef SCREEN_H
#define SCREEN_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <libavformat/avformat.h>

#include "common.h"

struct video_buffer;

struct screen {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    struct size frame_size;
    //used only in fullscreen mode to know the windowed window size
    struct size windowed_window_size;
    bool has_frame;
    bool fullscreen;
    bool no_window;
};

#define SCREEN_INITIALIZER {  \
    .window = NULL,           \
    .renderer = NULL,         \
    .texture = NULL,          \
    .frame_size = {           \
        .width = 0,           \
        .height = 0,          \
    },                        \
    .windowed_window_size = { \
        .width = 0,           \
        .height = 0,          \
    },                        \
    .has_frame = false,   \
    .fullscreen = false,  \
    .no_window = false,   \
}

// initialize default values
void
screen_init(struct screen *screen);

// initialize screen, create window, renderer and texture (window is hidden)
bool
screen_init_rendering(struct screen *screen, const char *device_name,
                      struct size frame_size, bool always_on_top);

// show the window
void
screen_show_window(struct screen *screen);

// destroy window, renderer and texture (if any)
void
screen_destroy(struct screen *screen);

// resize if necessary and write the rendered frame into the texture
bool
screen_update_frame(struct screen *screen, struct video_buffer *vb);

// render the texture to the renderer
void
screen_render(struct screen *screen);

// switch the fullscreen mode
void
screen_switch_fullscreen(struct screen *screen);

// resize window to optimal size (remove black borders)
void
screen_resize_to_fit(struct screen *screen);

// resize window to 1:1 (pixel-perfect)
void
screen_resize_to_pixel_perfect(struct screen *screen);

#endif
