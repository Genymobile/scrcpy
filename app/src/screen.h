#ifndef SCREEN_H
#define SCREEN_H

#include "common.h"

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <libavformat/avformat.h>

#include "controller.h"
#include "coords.h"
#include "fps_counter.h"
#include "input_manager.h"
#include "opengl.h"
#include "trait/key_processor.h"
#include "trait/frame_sink.h"
#include "trait/mouse_processor.h"
#include "video_buffer.h"

struct sc_screen {
    struct sc_frame_sink frame_sink; // frame sink trait

#ifndef NDEBUG
    bool open; // track the open/close state to assert correct behavior
#endif

    struct sc_input_manager im;
    struct sc_video_buffer vb;
    struct sc_fps_counter fps_counter;

    // The initial requested window properties
    struct {
        int16_t x;
        int16_t y;
        uint16_t width;
        uint16_t height;
        bool fullscreen;
        bool start_fps_counter;
    } req;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    struct sc_opengl gl;
    struct sc_size frame_size;
    struct sc_size content_size; // rotated frame_size

    bool resize_pending; // resize requested while fullscreen or maximized
    // The content size the last time the window was not maximized or
    // fullscreen (meaningful only when resize_pending is true)
    struct sc_size windowed_content_size;

    // client rotation: 0, 1, 2 or 3 (x90 degrees counterclockwise)
    unsigned rotation;
    // rectangle of the content (excluding black borders)
    struct SDL_Rect rect;
    bool has_frame;
    bool fullscreen;
    bool maximized;
    bool mipmaps;

    bool event_failed; // in case SDL_PushEvent() returned an error

    // To enable/disable mouse capture, a mouse capture key (LALT, LGUI or
    // RGUI) must be pressed. This variable tracks the pressed capture key.
    SDL_Keycode mouse_capture_key_pressed;

    AVFrame *frame;
};

struct sc_screen_params {
    struct sc_controller *controller;
    struct sc_file_pusher *fp;
    struct sc_key_processor *kp;
    struct sc_mouse_processor *mp;

    bool forward_all_clicks;
    bool legacy_paste;
    bool clipboard_autosync;
    const struct sc_shortcut_mods *shortcut_mods;

    const char *window_title;
    struct sc_size frame_size;
    bool always_on_top;

    int16_t window_x; // accepts SC_WINDOW_POSITION_UNDEFINED
    int16_t window_y; // accepts SC_WINDOW_POSITION_UNDEFINED
    uint16_t window_width;
    uint16_t window_height;

    bool window_borderless;

    uint8_t rotation;
    bool mipmaps;

    bool fullscreen;
    bool start_fps_counter;

    sc_tick buffering_time;
};

// initialize screen, create window, renderer and texture (window is hidden)
bool
sc_screen_init(struct sc_screen *screen, const struct sc_screen_params *params);

// request to interrupt any inner thread
// must be called before screen_join()
void
sc_screen_interrupt(struct sc_screen *screen);

// join any inner thread
void
sc_screen_join(struct sc_screen *screen);

// destroy window, renderer and texture (if any)
void
sc_screen_destroy(struct sc_screen *screen);

// hide the window
//
// It is used to hide the window immediately on closing without waiting for
// screen_destroy()
void
sc_screen_hide_window(struct sc_screen *screen);

// switch the fullscreen mode
void
sc_screen_switch_fullscreen(struct sc_screen *screen);

// resize window to optimal size (remove black borders)
void
sc_screen_resize_to_fit(struct sc_screen *screen);

// resize window to 1:1 (pixel-perfect)
void
sc_screen_resize_to_pixel_perfect(struct sc_screen *screen);

// set the display rotation (0, 1, 2 or 3, x90 degrees counterclockwise)
void
sc_screen_set_rotation(struct sc_screen *screen, unsigned rotation);

// react to SDL events
void
sc_screen_handle_event(struct sc_screen *screen, SDL_Event *event);

// convert point from window coordinates to frame coordinates
// x and y are expressed in pixels
struct sc_point
sc_screen_convert_window_to_frame_coords(struct sc_screen *screen,
                                        int32_t x, int32_t y);

// convert point from drawable coordinates to frame coordinates
// x and y are expressed in pixels
struct sc_point
sc_screen_convert_drawable_to_frame_coords(struct sc_screen *screen,
                                          int32_t x, int32_t y);

// Convert coordinates from window to drawable.
// Events are expressed in window coordinates, but content is expressed in
// drawable coordinates. They are the same if HiDPI scaling is 1, but differ
// otherwise.
void
sc_screen_hidpi_scale_coords(struct sc_screen *screen, int32_t *x, int32_t *y);

#endif
