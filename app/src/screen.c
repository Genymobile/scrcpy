#include "screen.h"

#include <assert.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "events.h"
#include "icon.h"
#include "options.h"
#include "video_buffer.h"
#include "util/log.h"

#define DISPLAY_MARGINS 96

#define DOWNCAST(SINK) container_of(SINK, struct sc_screen, frame_sink)

static inline struct sc_size
get_rotated_size(struct sc_size size, int rotation) {
    struct sc_size rotated_size;
    if (rotation & 1) {
        rotated_size.width = size.height;
        rotated_size.height = size.width;
    } else {
        rotated_size.width = size.width;
        rotated_size.height = size.height;
    }
    return rotated_size;
}

// get the window size in a struct sc_size
static struct sc_size
get_window_size(const struct sc_screen *screen) {
    int width;
    int height;
    SDL_GetWindowSize(screen->window, &width, &height);

    struct sc_size size;
    size.width = width;
    size.height = height;
    return size;
}

static struct sc_point
get_window_position(const struct sc_screen *screen) {
    int x;
    int y;
    SDL_GetWindowPosition(screen->window, &x, &y);

    struct sc_point point;
    point.x = x;
    point.y = y;
    return point;
}

// set the window size to be applied when fullscreen is disabled
static void
set_window_size(struct sc_screen *screen, struct sc_size new_size) {
    assert(!screen->fullscreen);
    assert(!screen->maximized);
    SDL_SetWindowSize(screen->window, new_size.width, new_size.height);
}

// get the preferred display bounds (i.e. the screen bounds with some margins)
static bool
get_preferred_display_bounds(struct sc_size *bounds) {
    SDL_Rect rect;
    if (SDL_GetDisplayUsableBounds(0, &rect)) {
        LOGW("Could not get display usable bounds: %s", SDL_GetError());
        return false;
    }

    bounds->width = MAX(0, rect.w - DISPLAY_MARGINS);
    bounds->height = MAX(0, rect.h - DISPLAY_MARGINS);
    return true;
}

static bool
is_optimal_size(struct sc_size current_size, struct sc_size content_size) {
    // The size is optimal if we can recompute one dimension of the current
    // size from the other
    return current_size.height == current_size.width * content_size.height
                                                     / content_size.width
        || current_size.width == current_size.height * content_size.width
                                                     / content_size.height;
}

// return the optimal size of the window, with the following constraints:
//  - it attempts to keep at least one dimension of the current_size (i.e. it
//    crops the black borders)
//  - it keeps the aspect ratio
//  - it scales down to make it fit in the display_size
static struct sc_size
get_optimal_size(struct sc_size current_size, struct sc_size content_size,
                 bool within_display_bounds) {
    if (content_size.width == 0 || content_size.height == 0) {
        // avoid division by 0
        return current_size;
    }

    struct sc_size window_size;

    struct sc_size display_size;
    if (!within_display_bounds ||
            !get_preferred_display_bounds(&display_size)) {
        // do not constraint the size
        window_size = current_size;
    } else {
        window_size.width = MIN(current_size.width, display_size.width);
        window_size.height = MIN(current_size.height, display_size.height);
    }

    if (is_optimal_size(window_size, content_size)) {
        return window_size;
    }

    bool keep_width = content_size.width * window_size.height
                    > content_size.height * window_size.width;
    if (keep_width) {
        // remove black borders on top and bottom
        window_size.height = content_size.height * window_size.width
                           / content_size.width;
    } else {
        // remove black borders on left and right (or none at all if it already
        // fits)
        window_size.width = content_size.width * window_size.height
                          / content_size.height;
    }

    return window_size;
}

// initially, there is no current size, so use the frame size as current size
// req_width and req_height, if not 0, are the sizes requested by the user
static inline struct sc_size
get_initial_optimal_size(struct sc_size content_size, uint16_t req_width,
                         uint16_t req_height) {
    struct sc_size window_size;
    if (!req_width && !req_height) {
        window_size = get_optimal_size(content_size, content_size, true);
    } else {
        if (req_width) {
            window_size.width = req_width;
        } else {
            // compute from the requested height
            window_size.width = (uint32_t) req_height * content_size.width
                              / content_size.height;
        }
        if (req_height) {
            window_size.height = req_height;
        } else {
            // compute from the requested width
            window_size.height = (uint32_t) req_width * content_size.height
                               / content_size.width;
        }
    }
    return window_size;
}

static inline bool
sc_screen_is_relative_mode(struct sc_screen *screen) {
    // screen->im.mp may be NULL if --no-control
    return screen->im.mp && screen->im.mp->relative_mode;
}

static void
sc_screen_set_mouse_capture(struct sc_screen *screen, bool capture) {
#ifdef __APPLE__
    // Workaround for SDL bug on macOS:
    // <https://github.com/libsdl-org/SDL/issues/5340>
    if (capture) {
        int mouse_x, mouse_y;
        SDL_GetGlobalMouseState(&mouse_x, &mouse_y);

        int x, y, w, h;
        SDL_GetWindowPosition(screen->window, &x, &y);
        SDL_GetWindowSize(screen->window, &w, &h);

        bool outside_window = mouse_x < x || mouse_x >= x + w
                           || mouse_y < y || mouse_y >= y + h;
        if (outside_window) {
            SDL_WarpMouseInWindow(screen->window, w / 2, h / 2);
        }
    }
#else
    (void) screen;
#endif
    if (SDL_SetRelativeMouseMode(capture)) {
        LOGE("Could not set relative mouse mode to %s: %s",
             capture ? "true" : "false", SDL_GetError());
    }
}

static inline bool
sc_screen_get_mouse_capture(struct sc_screen *screen) {
    (void) screen;
    return SDL_GetRelativeMouseMode();
}

static inline void
sc_screen_toggle_mouse_capture(struct sc_screen *screen) {
    (void) screen;
    bool new_value = !sc_screen_get_mouse_capture(screen);
    sc_screen_set_mouse_capture(screen, new_value);
}

static void
sc_screen_update_content_rect(struct sc_screen *screen) {
    int dw;
    int dh;
    SDL_GL_GetDrawableSize(screen->window, &dw, &dh);

    struct sc_size content_size = screen->content_size;
    // The drawable size is the window size * the HiDPI scale
    struct sc_size drawable_size = {dw, dh};

    SDL_Rect *rect = &screen->rect;

    if (is_optimal_size(drawable_size, content_size)) {
        rect->x = 0;
        rect->y = 0;
        rect->w = drawable_size.width;
        rect->h = drawable_size.height;
        return;
    }

    bool keep_width = content_size.width * drawable_size.height
                    > content_size.height * drawable_size.width;
    if (keep_width) {
        rect->x = 0;
        rect->w = drawable_size.width;
        rect->h = drawable_size.width * content_size.height
                                      / content_size.width;
        rect->y = (drawable_size.height - rect->h) / 2;
    } else {
        rect->y = 0;
        rect->h = drawable_size.height;
        rect->w = drawable_size.height * content_size.width
                                       / content_size.height;
        rect->x = (drawable_size.width - rect->w) / 2;
    }
}

static inline SDL_Texture *
create_texture(struct sc_screen *screen) {
    SDL_Renderer *renderer = screen->renderer;
    struct sc_size size = screen->frame_size;
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             size.width, size.height);
    if (!texture) {
        return NULL;
    }

    if (screen->mipmaps) {
        struct sc_opengl *gl = &screen->gl;

        SDL_GL_BindTexture(texture, NULL, NULL);

        // Enable trilinear filtering for downscaling
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          GL_LINEAR_MIPMAP_LINEAR);
        gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.f);

        SDL_GL_UnbindTexture(texture);
    }

    return texture;
}

// render the texture to the renderer
//
// Set the update_content_rect flag if the window or content size may have
// changed, so that the content rectangle is recomputed
static void
sc_screen_render(struct sc_screen *screen, bool update_content_rect) {
    if (update_content_rect) {
        sc_screen_update_content_rect(screen);
    }

    SDL_RenderClear(screen->renderer);
    if (screen->rotation == 0) {
        SDL_RenderCopy(screen->renderer, screen->texture, NULL, &screen->rect);
    } else {
        // rotation in RenderCopyEx() is clockwise, while screen->rotation is
        // counterclockwise (to be consistent with --lock-video-orientation)
        int cw_rotation = (4 - screen->rotation) % 4;
        double angle = 90 * cw_rotation;

        SDL_Rect *dstrect = NULL;
        SDL_Rect rect;
        if (screen->rotation & 1) {
            rect.x = screen->rect.x + (screen->rect.w - screen->rect.h) / 2;
            rect.y = screen->rect.y + (screen->rect.h - screen->rect.w) / 2;
            rect.w = screen->rect.h;
            rect.h = screen->rect.w;
            dstrect = &rect;
        } else {
            assert(screen->rotation == 2);
            dstrect = &screen->rect;
        }

        SDL_RenderCopyEx(screen->renderer, screen->texture, NULL, dstrect,
                         angle, NULL, 0);
    }
    SDL_RenderPresent(screen->renderer);
}


#if defined(__APPLE__)
# define CONTINUOUS_RESIZING_WORKAROUND
#endif

#ifdef CONTINUOUS_RESIZING_WORKAROUND
// On Windows and MacOS, resizing blocks the event loop, so resizing events are
// not triggered. On MacOS, as a workaround, handle them in an event handler
// (it does not work for Windows unfortunately).
//
// <https://bugzilla.libsdl.org/show_bug.cgi?id=2077>
// <https://stackoverflow.com/a/40693139/1987178>
static int
event_watcher(void *data, SDL_Event *event) {
    struct sc_screen *screen = data;
    if (event->type == SDL_WINDOWEVENT
            && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        // In practice, it seems to always be called from the same thread in
        // that specific case. Anyway, it's just a workaround.
        sc_screen_render(screen, true);
    }
    return 0;
}
#endif

static bool
sc_screen_frame_sink_open(struct sc_frame_sink *sink) {
    struct sc_screen *screen = DOWNCAST(sink);
    (void) screen;
#ifndef NDEBUG
    screen->open = true;
#endif

    // nothing to do, the screen is already open on the main thread
    return true;
}

static void
sc_screen_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_screen *screen = DOWNCAST(sink);
    (void) screen;
#ifndef NDEBUG
    screen->open = false;
#endif

    // nothing to do, the screen lifecycle is not managed by the frame producer
}

static bool
sc_screen_frame_sink_push(struct sc_frame_sink *sink, const AVFrame *frame) {
    struct sc_screen *screen = DOWNCAST(sink);
    return sc_video_buffer_push(&screen->vb, frame);
}

static void
sc_video_buffer_on_new_frame(struct sc_video_buffer *vb, bool previous_skipped,
                             void *userdata) {
    (void) vb;
    struct sc_screen *screen = userdata;

    // event_failed implies previous_skipped (the previous frame may not have
    // been consumed if the event was not sent)
    assert(!screen->event_failed || previous_skipped);

    bool need_new_event;
    if (previous_skipped) {
        sc_fps_counter_add_skipped_frame(&screen->fps_counter);
        // The EVENT_NEW_FRAME triggered for the previous frame will consume
        // this new frame instead, unless the previous event failed
        need_new_event = screen->event_failed;
    } else {
        need_new_event = true;
    }

    if (need_new_event) {
        static SDL_Event new_frame_event = {
            .type = EVENT_NEW_FRAME,
        };

        // Post the event on the UI thread
        int ret = SDL_PushEvent(&new_frame_event);
        if (ret < 0) {
            LOGW("Could not post new frame event: %s", SDL_GetError());
            screen->event_failed = true;
        } else {
            screen->event_failed = false;
        }
    }
}

bool
sc_screen_init(struct sc_screen *screen,
               const struct sc_screen_params *params) {
    screen->resize_pending = false;
    screen->has_frame = false;
    screen->fullscreen = false;
    screen->maximized = false;
    screen->event_failed = false;
    screen->mouse_capture_key_pressed = 0;

    screen->req.x = params->window_x;
    screen->req.y = params->window_y;
    screen->req.width = params->window_width;
    screen->req.height = params->window_height;
    screen->req.fullscreen = params->fullscreen;
    screen->req.start_fps_counter = params->start_fps_counter;

    static const struct sc_video_buffer_callbacks cbs = {
        .on_new_frame = sc_video_buffer_on_new_frame,
    };

    bool ok = sc_video_buffer_init(&screen->vb, params->buffering_time, &cbs,
                                   screen);
    if (!ok) {
        return false;
    }

    ok = sc_video_buffer_start(&screen->vb);
    if (!ok) {
        goto error_destroy_video_buffer;
    }

    if (!sc_fps_counter_init(&screen->fps_counter)) {
        goto error_stop_and_join_video_buffer;
    }

    screen->frame_size = params->frame_size;
    screen->rotation = params->rotation;
    if (screen->rotation) {
        LOGI("Initial display rotation set to %u", screen->rotation);
    }
    struct sc_size content_size =
        get_rotated_size(screen->frame_size, screen->rotation);
    screen->content_size = content_size;

    uint32_t window_flags = SDL_WINDOW_HIDDEN
                          | SDL_WINDOW_RESIZABLE
                          | SDL_WINDOW_ALLOW_HIGHDPI;
    if (params->always_on_top) {
        window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    if (params->window_borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    // The window will be positioned and sized on first video frame
    screen->window =
        SDL_CreateWindow(params->window_title, 0, 0, 0, 0, window_flags);
    if (!screen->window) {
        LOGE("Could not create window: %s", SDL_GetError());
        goto error_destroy_fps_counter;
    }

    screen->renderer = SDL_CreateRenderer(screen->window, -1,
                                          SDL_RENDERER_ACCELERATED);
    if (!screen->renderer) {
        LOGE("Could not create renderer: %s", SDL_GetError());
        goto error_destroy_window;
    }

    SDL_RendererInfo renderer_info;
    int r = SDL_GetRendererInfo(screen->renderer, &renderer_info);
    const char *renderer_name = r ? NULL : renderer_info.name;
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    screen->mipmaps = false;

    // starts with "opengl"
    bool use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (use_opengl) {
        struct sc_opengl *gl = &screen->gl;
        sc_opengl_init(gl);

        LOGI("OpenGL version: %s", gl->version);

        if (params->mipmaps) {
            bool supports_mipmaps =
                sc_opengl_version_at_least(gl, 3, 0, /* OpenGL 3.0+ */
                                               2, 0  /* OpenGL ES 2.0+ */);
            if (supports_mipmaps) {
                LOGI("Trilinear filtering enabled");
                screen->mipmaps = true;
            } else {
                LOGW("Trilinear filtering disabled "
                     "(OpenGL 3.0+ or ES 2.0+ required)");
            }
        } else {
            LOGI("Trilinear filtering disabled");
        }
    } else if (params->mipmaps) {
        LOGD("Trilinear filtering disabled (not an OpenGL renderer)");
    }

    SDL_Surface *icon = scrcpy_icon_load();
    if (icon) {
        SDL_SetWindowIcon(screen->window, icon);
        scrcpy_icon_destroy(icon);
    } else {
        LOGW("Could not load icon");
    }

    LOGI("Initial texture: %" PRIu16 "x%" PRIu16, params->frame_size.width,
                                                  params->frame_size.height);
    screen->texture = create_texture(screen);
    if (!screen->texture) {
        LOGE("Could not create texture: %s", SDL_GetError());
        goto error_destroy_renderer;
    }

    screen->frame = av_frame_alloc();
    if (!screen->frame) {
        LOG_OOM();
        goto error_destroy_texture;
    }

    struct sc_input_manager_params im_params = {
        .controller = params->controller,
        .fp = params->fp,
        .screen = screen,
        .kp = params->kp,
        .mp = params->mp,
        .forward_all_clicks = params->forward_all_clicks,
        .legacy_paste = params->legacy_paste,
        .clipboard_autosync = params->clipboard_autosync,
        .shortcut_mods = params->shortcut_mods,
    };

    sc_input_manager_init(&screen->im, &im_params);

#ifdef CONTINUOUS_RESIZING_WORKAROUND
    SDL_AddEventWatch(event_watcher, screen);
#endif

    static const struct sc_frame_sink_ops ops = {
        .open = sc_screen_frame_sink_open,
        .close = sc_screen_frame_sink_close,
        .push = sc_screen_frame_sink_push,
    };

    screen->frame_sink.ops = &ops;

#ifndef NDEBUG
    screen->open = false;
#endif

    return true;

error_destroy_texture:
    SDL_DestroyTexture(screen->texture);
error_destroy_renderer:
    SDL_DestroyRenderer(screen->renderer);
error_destroy_window:
    SDL_DestroyWindow(screen->window);
error_destroy_fps_counter:
    sc_fps_counter_destroy(&screen->fps_counter);
error_stop_and_join_video_buffer:
    sc_video_buffer_stop(&screen->vb);
    sc_video_buffer_join(&screen->vb);
error_destroy_video_buffer:
    sc_video_buffer_destroy(&screen->vb);

    return false;
}

static void
sc_screen_show_initial_window(struct sc_screen *screen) {
    int x = screen->req.x != SC_WINDOW_POSITION_UNDEFINED
          ? screen->req.x : (int) SDL_WINDOWPOS_CENTERED;
    int y = screen->req.y != SC_WINDOW_POSITION_UNDEFINED
          ? screen->req.y : (int) SDL_WINDOWPOS_CENTERED;

    struct sc_size window_size =
        get_initial_optimal_size(screen->content_size, screen->req.width,
                                                       screen->req.height);

    set_window_size(screen, window_size);
    SDL_SetWindowPosition(screen->window, x, y);

    if (screen->req.fullscreen) {
        sc_screen_switch_fullscreen(screen);
    }

    if (screen->req.start_fps_counter) {
        sc_fps_counter_start(&screen->fps_counter);
    }

    SDL_ShowWindow(screen->window);
}

void
sc_screen_hide_window(struct sc_screen *screen) {
    SDL_HideWindow(screen->window);
}

void
sc_screen_interrupt(struct sc_screen *screen) {
    sc_video_buffer_stop(&screen->vb);
    sc_fps_counter_interrupt(&screen->fps_counter);
}

void
sc_screen_join(struct sc_screen *screen) {
    sc_video_buffer_join(&screen->vb);
    sc_fps_counter_join(&screen->fps_counter);
}

void
sc_screen_destroy(struct sc_screen *screen) {
#ifndef NDEBUG
    assert(!screen->open);
#endif
    av_frame_free(&screen->frame);
    SDL_DestroyTexture(screen->texture);
    SDL_DestroyRenderer(screen->renderer);
    SDL_DestroyWindow(screen->window);
    sc_fps_counter_destroy(&screen->fps_counter);
    sc_video_buffer_destroy(&screen->vb);
}

static void
resize_for_content(struct sc_screen *screen, struct sc_size old_content_size,
                   struct sc_size new_content_size) {
    struct sc_size window_size = get_window_size(screen);
    struct sc_size target_size = {
        .width = (uint32_t) window_size.width * new_content_size.width
                / old_content_size.width,
        .height = (uint32_t) window_size.height * new_content_size.height
                / old_content_size.height,
    };
    target_size = get_optimal_size(target_size, new_content_size, true);
    set_window_size(screen, target_size);
}

static void
set_content_size(struct sc_screen *screen, struct sc_size new_content_size) {
    if (!screen->fullscreen && !screen->maximized) {
        resize_for_content(screen, screen->content_size, new_content_size);
    } else if (!screen->resize_pending) {
        // Store the windowed size to be able to compute the optimal size once
        // fullscreen and maximized are disabled
        screen->windowed_content_size = screen->content_size;
        screen->resize_pending = true;
    }

    screen->content_size = new_content_size;
}

static void
apply_pending_resize(struct sc_screen *screen) {
    assert(!screen->fullscreen);
    assert(!screen->maximized);
    if (screen->resize_pending) {
        resize_for_content(screen, screen->windowed_content_size,
                                   screen->content_size);
        screen->resize_pending = false;
    }
}

void
sc_screen_set_rotation(struct sc_screen *screen, unsigned rotation) {
    assert(rotation < 4);
    if (rotation == screen->rotation) {
        return;
    }

    struct sc_size new_content_size =
        get_rotated_size(screen->frame_size, rotation);

    set_content_size(screen, new_content_size);

    screen->rotation = rotation;
    LOGI("Display rotation set to %u", rotation);

    sc_screen_render(screen, true);
}

// recreate the texture and resize the window if the frame size has changed
static bool
prepare_for_frame(struct sc_screen *screen, struct sc_size new_frame_size) {
    if (screen->frame_size.width != new_frame_size.width
            || screen->frame_size.height != new_frame_size.height) {
        // frame dimension changed, destroy texture
        SDL_DestroyTexture(screen->texture);

        screen->frame_size = new_frame_size;

        struct sc_size new_content_size =
            get_rotated_size(new_frame_size, screen->rotation);
        set_content_size(screen, new_content_size);

        sc_screen_update_content_rect(screen);

        LOGI("New texture: %" PRIu16 "x%" PRIu16,
                     screen->frame_size.width, screen->frame_size.height);
        screen->texture = create_texture(screen);
        if (!screen->texture) {
            LOGE("Could not create texture: %s", SDL_GetError());
            return false;
        }
    }

    return true;
}

// write the frame into the texture
static void
update_texture(struct sc_screen *screen, const AVFrame *frame) {
    SDL_UpdateYUVTexture(screen->texture, NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);

    if (screen->mipmaps) {
        SDL_GL_BindTexture(screen->texture, NULL, NULL);
        screen->gl.GenerateMipmap(GL_TEXTURE_2D);
        SDL_GL_UnbindTexture(screen->texture);
    }
}

static bool
sc_screen_update_frame(struct sc_screen *screen) {
    av_frame_unref(screen->frame);
    sc_video_buffer_consume(&screen->vb, screen->frame);
    AVFrame *frame = screen->frame;

    sc_fps_counter_add_rendered_frame(&screen->fps_counter);

    struct sc_size new_frame_size = {frame->width, frame->height};
    if (!prepare_for_frame(screen, new_frame_size)) {
        return false;
    }
    update_texture(screen, frame);

    if (!screen->has_frame) {
        screen->has_frame = true;
        // this is the very first frame, show the window
        sc_screen_show_initial_window(screen);

        if (sc_screen_is_relative_mode(screen)) {
            // Capture mouse on start
            sc_screen_set_mouse_capture(screen, true);
        }
    }

    sc_screen_render(screen, false);
    return true;
}

void
sc_screen_switch_fullscreen(struct sc_screen *screen) {
    uint32_t new_mode = screen->fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (SDL_SetWindowFullscreen(screen->window, new_mode)) {
        LOGW("Could not switch fullscreen mode: %s", SDL_GetError());
        return;
    }

    screen->fullscreen = !screen->fullscreen;
    if (!screen->fullscreen && !screen->maximized) {
        apply_pending_resize(screen);
    }

    LOGD("Switched to %s mode", screen->fullscreen ? "fullscreen" : "windowed");
    sc_screen_render(screen, true);
}

void
sc_screen_resize_to_fit(struct sc_screen *screen) {
    if (screen->fullscreen || screen->maximized) {
        return;
    }

    struct sc_point point = get_window_position(screen);
    struct sc_size window_size = get_window_size(screen);

    struct sc_size optimal_size =
        get_optimal_size(window_size, screen->content_size, false);

    // Center the window related to the device screen
    assert(optimal_size.width <= window_size.width);
    assert(optimal_size.height <= window_size.height);
    uint32_t new_x = point.x + (window_size.width - optimal_size.width) / 2;
    uint32_t new_y = point.y + (window_size.height - optimal_size.height) / 2;

    SDL_SetWindowSize(screen->window, optimal_size.width, optimal_size.height);
    SDL_SetWindowPosition(screen->window, new_x, new_y);
    LOGD("Resized to optimal size: %ux%u", optimal_size.width,
                                           optimal_size.height);
}

void
sc_screen_resize_to_pixel_perfect(struct sc_screen *screen) {
    if (screen->fullscreen) {
        return;
    }

    if (screen->maximized) {
        SDL_RestoreWindow(screen->window);
        screen->maximized = false;
    }

    struct sc_size content_size = screen->content_size;
    SDL_SetWindowSize(screen->window, content_size.width, content_size.height);
    LOGD("Resized to pixel-perfect: %ux%u", content_size.width,
                                            content_size.height);
}

static inline bool
sc_screen_is_mouse_capture_key(SDL_Keycode key) {
    return key == SDLK_LALT || key == SDLK_LGUI || key == SDLK_RGUI;
}

void
sc_screen_handle_event(struct sc_screen *screen, SDL_Event *event) {
    bool relative_mode = sc_screen_is_relative_mode(screen);

    switch (event->type) {
        case EVENT_NEW_FRAME: {
            bool ok = sc_screen_update_frame(screen);
            if (!ok) {
                LOGW("Frame update failed\n");
            }
            return;
        }
        case SDL_WINDOWEVENT:
            if (!screen->has_frame) {
                // Do nothing
                return;
            }
            switch (event->window.event) {
                case SDL_WINDOWEVENT_EXPOSED:
                    sc_screen_render(screen, true);
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    sc_screen_render(screen, true);
                    break;
                case SDL_WINDOWEVENT_MAXIMIZED:
                    screen->maximized = true;
                    break;
                case SDL_WINDOWEVENT_RESTORED:
                    if (screen->fullscreen) {
                        // On Windows, in maximized+fullscreen, disabling
                        // fullscreen mode unexpectedly triggers the "restored"
                        // then "maximized" events, leaving the window in a
                        // weird state (maximized according to the events, but
                        // not maximized visually).
                        break;
                    }
                    screen->maximized = false;
                    apply_pending_resize(screen);
                    sc_screen_render(screen, true);
                    break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    if (relative_mode) {
                        sc_screen_set_mouse_capture(screen, false);
                    }
                    break;
            }
            return;
        case SDL_KEYDOWN:
            if (relative_mode) {
                SDL_Keycode key = event->key.keysym.sym;
                if (sc_screen_is_mouse_capture_key(key)) {
                    if (!screen->mouse_capture_key_pressed) {
                        screen->mouse_capture_key_pressed = key;
                    } else {
                        // Another mouse capture key has been pressed, cancel
                        // mouse (un)capture
                        screen->mouse_capture_key_pressed = 0;
                    }
                    // Mouse capture keys are never forwarded to the device
                    return;
                }
            }
            break;
        case SDL_KEYUP:
            if (relative_mode) {
                SDL_Keycode key = event->key.keysym.sym;
                SDL_Keycode cap = screen->mouse_capture_key_pressed;
                screen->mouse_capture_key_pressed = 0;
                if (sc_screen_is_mouse_capture_key(key)) {
                    if (key == cap) {
                        // A mouse capture key has been pressed then released:
                        // toggle the capture mouse mode
                        sc_screen_toggle_mouse_capture(screen);
                    }
                    // Mouse capture keys are never forwarded to the device
                    return;
                }
            }
            break;
        case SDL_MOUSEWHEEL:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
            if (relative_mode && !sc_screen_get_mouse_capture(screen)) {
                // Do not forward to input manager, the mouse will be captured
                // on SDL_MOUSEBUTTONUP
                return;
            }
            break;
        case SDL_FINGERMOTION:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
            if (relative_mode) {
                // Touch events are not compatible with relative mode
                // (coordinates are not relative)
                return;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (relative_mode && !sc_screen_get_mouse_capture(screen)) {
                sc_screen_set_mouse_capture(screen, true);
                return;
            }
            break;
    }

    sc_input_manager_handle_event(&screen->im, event);
}

struct sc_point
sc_screen_convert_drawable_to_frame_coords(struct sc_screen *screen,
                                           int32_t x, int32_t y) {
    unsigned rotation = screen->rotation;
    assert(rotation < 4);

    int32_t w = screen->content_size.width;
    int32_t h = screen->content_size.height;


    x = (int64_t) (x - screen->rect.x) * w / screen->rect.w;
    y = (int64_t) (y - screen->rect.y) * h / screen->rect.h;

    // rotate
    struct sc_point result;
    switch (rotation) {
        case 0:
            result.x = x;
            result.y = y;
            break;
        case 1:
            result.x = h - y;
            result.y = x;
            break;
        case 2:
            result.x = w - x;
            result.y = h - y;
            break;
        default:
            assert(rotation == 3);
            result.x = y;
            result.y = w - x;
            break;
    }
    return result;
}

struct sc_point
sc_screen_convert_window_to_frame_coords(struct sc_screen *screen,
                                         int32_t x, int32_t y) {
    sc_screen_hidpi_scale_coords(screen, &x, &y);
    return sc_screen_convert_drawable_to_frame_coords(screen, x, y);
}

void
sc_screen_hidpi_scale_coords(struct sc_screen *screen, int32_t *x, int32_t *y) {
    // take the HiDPI scaling (dw/ww and dh/wh) into account
    int ww, wh, dw, dh;
    SDL_GetWindowSize(screen->window, &ww, &wh);
    SDL_GL_GetDrawableSize(screen->window, &dw, &dh);

    // scale for HiDPI (64 bits for intermediate multiplications)
    *x = (int64_t) *x * dw / ww;
    *y = (int64_t) *y * dh / wh;
}
