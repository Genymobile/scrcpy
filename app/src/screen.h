#ifndef SCREEN_H
#define SCREEN_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <libavformat/avformat.h>

#include "config.h"
#include "common.h"
#include "opengl.h"

#define WINDOW_POSITION_UNDEFINED (-0x8000)

struct video_buffer;

struct screen {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    bool use_opengl;
    struct sc_opengl gl;
    struct size frame_size;
    struct size content_size; // rotated frame_size

    bool resize_pending; // resize requested while fullscreen or maximized
    // The content size the last time the window was not maximized or
    // fullscreen (meaningful only when resize_pending is true)
    struct size windowed_content_size;

    // client rotation: 0, 1, 2 or 3 (x90 degrees counterclockwise)
    unsigned rotation;
    // rectangle of the content (excluding black borders)
    struct SDL_Rect rect;
    bool has_frame;
    bool fullscreen;
    bool maximized;
    bool no_window;
    bool mipmaps;
};

#define SCREEN_INITIALIZER { \
    .window = NULL, \
    .renderer = NULL, \
    .texture = NULL, \
    .use_opengl = false, \
    .gl = {0}, \
    .frame_size = { \
        .width = 0, \
        .height = 0, \
    }, \
    .content_size = { \
        .width = 0, \
        .height = 0, \
    }, \
    .resize_pending = false, \
    .windowed_content_size = { \
        .width = 0, \
        .height = 0, \
    }, \
    .rotation = 0, \
    .rect = { \
        .x = 0, \
        .y = 0, \
        .w = 0, \
        .h = 0, \
    }, \
    .has_frame = false, \
    .fullscreen = false, \
    .maximized = false, \
    .no_window = false, \
    .mipmaps = false, \
}

// initialize default values
void
screen_init(struct screen *screen);

// initialize screen, create window, renderer and texture (window is hidden)
// window_x and window_y accept WINDOW_POSITION_UNDEFINED
bool
screen_init_rendering(struct screen *screen, const char *window_title,
                      struct size frame_size, bool always_on_top,
                      int16_t window_x, int16_t window_y, uint16_t window_width,
                      uint16_t window_height, bool window_borderless,
                      uint8_t rotation, bool mipmaps);

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
//
// Set the update_content_rect flag if the window or content size may have
// changed, so that the content rectangle is recomputed
void
screen_render(struct screen *screen, bool update_content_rect);

// switch the fullscreen mode
void
screen_switch_fullscreen(struct screen *screen);

// resize window to optimal size (remove black borders)
void
screen_resize_to_fit(struct screen *screen);

// resize window to 1:1 (pixel-perfect)
void
screen_resize_to_pixel_perfect(struct screen *screen);

// set the display rotation (0, 1, 2 or 3, x90 degrees counterclockwise)
void
screen_set_rotation(struct screen *screen, unsigned rotation);

// react to window events
void
screen_handle_window_event(struct screen *screen, const SDL_WindowEvent *event);

// convert point from window coordinates to frame coordinates
// x and y are expressed in pixels
struct point
screen_convert_to_frame_coords(struct screen *screen, int32_t x, int32_t y);

// Convert coordinates from window to drawable.
// Events are expressed in window coordinates, but content is expressed in
// drawable coordinates. They are the same if HiDPI scaling is 1, but differ
// otherwise.
void
screen_hidpi_scale_coords(struct screen *screen, int32_t *x, int32_t *y);

#endif
