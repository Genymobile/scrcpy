#ifndef SCREEN_H
#define SCREEN_H

#include "common.h"

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <libavformat/avformat.h>

#include "coords.h"
#include "fps_counter.h"
#include "opengl.h"
#include "trait/frame_sink.h"
#include "video_buffer.h"

struct screen {
    struct sc_frame_sink frame_sink; // frame sink trait

#ifndef NDEBUG
    bool open; // track the open/close state to assert correct behavior
#endif

    struct sc_video_buffer vb;
    struct fps_counter fps_counter;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
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
    bool mipmaps;

    AVFrame *frame;
};

struct screen_params {
    const char *window_title;
    struct size frame_size;
    bool always_on_top;

    int16_t window_x;
    int16_t window_y;
    uint16_t window_width;  // accepts SC_WINDOW_POSITION_UNDEFINED
    uint16_t window_height; // accepts SC_WINDOW_POSITION_UNDEFINED

    bool window_borderless;

    uint8_t rotation;
    bool mipmaps;

    bool fullscreen;

    sc_tick buffering_time;
};

// initialize screen, create window, renderer and texture (window is hidden)
bool
screen_init(struct screen *screen, const struct screen_params *params);

// request to interrupt any inner thread
// must be called before screen_join()
void
screen_interrupt(struct screen *screen);

// join any inner thread
void
screen_join(struct screen *screen);

// destroy window, renderer and texture (if any)
void
screen_destroy(struct screen *screen);

// hide the window
//
// It is used to hide the window immediately on closing without waiting for
// screen_destroy()
void
screen_hide_window(struct screen *screen);

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

// react to SDL events
bool
screen_handle_event(struct screen *screen, SDL_Event *event);

// convert point from window coordinates to frame coordinates
// x and y are expressed in pixels
struct point
screen_convert_window_to_frame_coords(struct screen *screen,
                                      int32_t x, int32_t y);

// convert point from drawable coordinates to frame coordinates
// x and y are expressed in pixels
struct point
screen_convert_drawable_to_frame_coords(struct screen *screen,
                                        int32_t x, int32_t y);

// Convert coordinates from window to drawable.
// Events are expressed in window coordinates, but content is expressed in
// drawable coordinates. They are the same if HiDPI scaling is 1, but differ
// otherwise.
void
screen_hidpi_scale_coords(struct screen *screen, int32_t *x, int32_t *y);

#endif
