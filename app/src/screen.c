#include "screen.h"

#include <assert.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "events.h"
#include "icon.h"
#include "options.h"
#include "util/log.h"
#include "util/sdl.h"

#define DISPLAY_MARGINS 96

#define DOWNCAST(SINK) container_of(SINK, struct sc_screen, frame_sink)

static inline struct sc_size
get_oriented_size(struct sc_size size, enum sc_orientation orientation) {
    struct sc_size oriented_size;
    if (sc_orientation_is_swap(orientation)) {
        oriented_size.width = size.height;
        oriented_size.height = size.width;
    } else {
        oriented_size.width = size.width;
        oriented_size.height = size.height;
    }
    return oriented_size;
}

static inline bool
is_windowed(struct sc_screen *screen) {
    return !(SDL_GetWindowFlags(screen->window) & (SDL_WINDOW_FULLSCREEN
                                                 | SDL_WINDOW_MINIMIZED
                                                 | SDL_WINDOW_MAXIMIZED));
}

// get the preferred display bounds (i.e. the screen bounds with some margins)
static bool
get_preferred_display_bounds(struct sc_size *bounds) {
    SDL_Rect rect;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (!display) {
        LOGW("Could not get primary display: %s", SDL_GetError());
        return false;
    }

    bool ok = SDL_GetDisplayUsableBounds(display, &rect);
    if (!ok) {
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
compute_content_rect(struct sc_size render_size, struct sc_size content_size,
                     bool can_upscale, SDL_FRect *rect) {
    if (is_optimal_size(render_size, content_size)) {
        rect->x = 0;
        rect->y = 0;
        rect->w = render_size.width;
        rect->h = render_size.height;
        return;
    }

    if (!can_upscale && content_size.width <= render_size.width
                     && content_size.height <= render_size.height) {
        // Center without upscaling
        rect->x = (render_size.width - content_size.width) / 2.f;
        rect->y = (render_size.height - content_size.height) / 2.f;
        rect->w = content_size.width;
        rect->h = content_size.height;
        return;
    }

    bool keep_width = content_size.width * render_size.height
                    > content_size.height * render_size.width;
    if (keep_width) {
        rect->x = 0;
        rect->w = render_size.width;
        rect->h = (float) render_size.width * content_size.height
                                            / content_size.width;
        rect->y = (render_size.height - rect->h) / 2.f;
    } else {
        rect->y = 0;
        rect->h = render_size.height;
        rect->w = (float) render_size.height * content_size.width
                                             / content_size.height;
        rect->x = (render_size.width - rect->w) / 2.f;
    }
}

static void
sc_screen_update_content_rect(struct sc_screen *screen) {
    // Only upscale video frames, not icon
    bool can_upscale = screen->video;

    struct sc_size render_size =
        sc_sdl_get_render_output_size(screen->renderer);
    compute_content_rect(render_size, screen->content_size, can_upscale,
                         &screen->rect);
}

// render the texture to the renderer
//
// Set the update_content_rect flag if the window or content size may have
// changed, so that the content rectangle is recomputed
static void
sc_screen_render(struct sc_screen *screen, bool update_content_rect) {
    assert(!screen->video || screen->has_video_window);

    if (update_content_rect) {
        sc_screen_update_content_rect(screen);
    }

    SDL_Renderer *renderer = screen->renderer;
    sc_sdl_render_clear(renderer);

    bool ok = false;
    SDL_Texture *texture = screen->tex.texture;
    if (!texture) {
        LOGW("No texture to render");
        goto end;
    }

    SDL_FRect *geometry = &screen->rect;
    enum sc_orientation orientation = screen->orientation;

    if (orientation == SC_ORIENTATION_0) {
        ok = SDL_RenderTexture(renderer, texture, NULL, geometry);
    } else {
        unsigned cw_rotation = sc_orientation_get_rotation(orientation);
        double angle = 90 * cw_rotation;

        const SDL_FRect *dstrect = NULL;
        SDL_FRect rect;
        if (sc_orientation_is_swap(orientation)) {
            rect.x = geometry->x + (geometry->w - geometry->h) / 2.f;
            rect.y = geometry->y + (geometry->h - geometry->w) / 2.f;
            rect.w = geometry->h;
            rect.h = geometry->w;
            dstrect = &rect;
        } else {
            dstrect = geometry;
        }

        SDL_FlipMode flip = sc_orientation_is_mirror(orientation)
                              ? SDL_FLIP_HORIZONTAL : 0;

        ok = SDL_RenderTextureRotated(renderer, texture, NULL, dstrect, angle,
                                      NULL, flip);
    }

    if (!ok) {
        LOGE("Could not render texture: %s", SDL_GetError());
    }

end:
    sc_sdl_render_present(renderer);
}

#if defined(__APPLE__) || defined(_WIN32)
# define CONTINUOUS_RESIZING_WORKAROUND
#endif

#ifdef CONTINUOUS_RESIZING_WORKAROUND
// On Windows and MacOS, resizing blocks the event loop, so resizing events are
// not triggered. As a workaround, handle them in an event handler.
//
// <https://bugzilla.libsdl.org/show_bug.cgi?id=2077>
// <https://stackoverflow.com/a/40693139/1987178>
static bool
event_watcher(void *data, SDL_Event *event) {
    struct sc_screen *screen = data;
    assert(screen->video);

    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        // In practice, it seems to always be called from the same thread in
        // that specific case. Anyway, it's just a workaround.
        sc_screen_render(screen, true);
    }

    return true;
}
#endif

static bool
sc_screen_frame_sink_open(struct sc_frame_sink *sink,
                          const AVCodecContext *ctx,
                          const struct sc_stream_session *session) {
    assert(ctx->pix_fmt == AV_PIX_FMT_YUV420P);
    (void) session;

    struct sc_screen *screen = DOWNCAST(sink);
    (void) screen;

    if (ctx->width <= 0 || ctx->width > 0xFFFF
            || ctx->height <= 0 || ctx->height > 0xFFFF) {
        LOGE("Invalid video size: %dx%d", ctx->width, ctx->height);
        return false;
    }

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
    assert(screen->video);

    bool previous_skipped;
    bool ok = sc_frame_buffer_push(&screen->fb, frame, &previous_skipped);
    if (!ok) {
        return false;
    }

    if (previous_skipped) {
        sc_fps_counter_add_skipped_frame(&screen->fps_counter);
        // The SC_EVENT_NEW_FRAME triggered for the previous frame will consume
        // this new frame instead
    } else {
        // Post the event on the UI thread
        bool ok = sc_push_event(SC_EVENT_NEW_FRAME);
        if (!ok) {
            return false;
        }
    }

    return true;
}

bool
sc_screen_init(struct sc_screen *screen,
               const struct sc_screen_params *params) {
    screen->resize_pending = false;
    screen->has_frame = false;
    screen->has_video_window = false;
    screen->paused = false;
    screen->resume_frame = NULL;
    screen->orientation = SC_ORIENTATION_0;

    screen->video = params->video;
    screen->camera = params->camera;

    screen->req.x = params->window_x;
    screen->req.y = params->window_y;
    screen->req.width = params->window_width;
    screen->req.height = params->window_height;
    screen->req.fullscreen = params->fullscreen;
    screen->req.start_fps_counter = params->start_fps_counter;

    bool ok = sc_frame_buffer_init(&screen->fb);
    if (!ok) {
        return false;
    }

    if (!sc_fps_counter_init(&screen->fps_counter)) {
        goto error_destroy_frame_buffer;
    }

    if (screen->video) {
        screen->orientation = params->orientation;
        if (screen->orientation != SC_ORIENTATION_0) {
            LOGI("Initial display orientation set to %s",
                 sc_orientation_get_name(screen->orientation));
        }
    }

    // Always create the window hidden to prevent blinking during initialization
    uint32_t window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (params->always_on_top) {
        window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    if (params->window_borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }
    if (params->video) {
        // The window will be shown on first frame
        window_flags |= SDL_WINDOW_RESIZABLE;
    }

    const char *title = params->window_title;
    assert(title);

    int x = SDL_WINDOWPOS_UNDEFINED;
    int y = SDL_WINDOWPOS_UNDEFINED;
    int width = 256;
    int height = 256;
    if (params->window_x != SC_WINDOW_POSITION_UNDEFINED) {
        x = params->window_x;
    }
    if (params->window_y != SC_WINDOW_POSITION_UNDEFINED) {
        y = params->window_y;
    }
    if (params->window_width) {
        width = params->window_width;
    }
    if (params->window_height) {
        height = params->window_height;
    }

    // The window will be positioned and sized on first video frame
    screen->window =
        sc_sdl_create_window(title, x, y, width, height, window_flags);
    if (!screen->window) {
        LOGE("Could not create window: %s", SDL_GetError());
        goto error_destroy_fps_counter;
    }

    screen->renderer = SDL_CreateRenderer(screen->window, NULL);
    if (!screen->renderer) {
        LOGE("Could not create renderer: %s", SDL_GetError());
        goto error_destroy_window;
    }

#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
    screen->gl_context = NULL;

    // starts with "opengl"
    const char *renderer_name = SDL_GetRendererName(screen->renderer);
    bool use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (use_opengl) {
        // Persuade macOS to give us something better than OpenGL 2.1.
        // If we create a Core Profile context, we get the best OpenGL version.
        bool ok = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                      SDL_GL_CONTEXT_PROFILE_CORE);
        if (!ok) {
            LOGW("Could not set a GL Core Profile Context");
        }

        LOGD("Creating OpenGL Core Profile context");
        screen->gl_context = SDL_GL_CreateContext(screen->window);
        if (!screen->gl_context) {
            LOGE("Could not create OpenGL context: %s", SDL_GetError());
            goto error_destroy_renderer;
        }
    }
#endif

    bool mipmaps = params->video;
    ok = sc_texture_init(&screen->tex, screen->renderer, mipmaps);
    if (!ok) {
        goto error_destroy_renderer;
    }

    ok = SDL_StartTextInput(screen->window);
    if (!ok) {
        LOGE("Could not enable text input: %s", SDL_GetError());
        goto error_destroy_texture;
    }

    SDL_Surface *icon = scrcpy_icon_load();
    if (icon) {
        if (!SDL_SetWindowIcon(screen->window, icon)) {
            LOGW("Could not set window icon: %s", SDL_GetError());
        }

        if (!params->video) {
            screen->content_size.width = icon->w;
            screen->content_size.height = icon->h;
            ok = sc_texture_set_from_surface(&screen->tex, icon);
            if (!ok) {
                LOGE("Could not set icon: %s", SDL_GetError());
            }
        }

        scrcpy_icon_destroy(icon);
    } else {
        // not fatal
        LOGE("Could not load icon");

        if (!params->video) {
            // Make sure the content size is initialized
            screen->content_size.width = 256;
            screen->content_size.height = 256;
        }
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
        .gp = params->gp,
        .camera = params->camera,
        .mouse_bindings = params->mouse_bindings,
        .legacy_paste = params->legacy_paste,
        .clipboard_autosync = params->clipboard_autosync,
        .shortcut_mods = params->shortcut_mods,
    };

    sc_input_manager_init(&screen->im, &im_params);

    // Initialize even if not used for simplicity
    sc_mouse_capture_init(&screen->mc, screen->window, params->shortcut_mods);

#ifdef CONTINUOUS_RESIZING_WORKAROUND
    if (screen->video) {
        ok = SDL_AddEventWatch(event_watcher, screen);
        if (!ok) {
            LOGW("Could not add event watcher for continuous resizing: %s",
                 SDL_GetError());
        }
    }
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

    if (!screen->video) {
        // Show the window immediately
        sc_sdl_show_window(screen->window);

        if (sc_screen_is_relative_mode(screen)) {
            // Capture mouse immediately if video mirroring is disabled
            sc_mouse_capture_set_active(&screen->mc, true);
        }
    }

    return true;

error_destroy_texture:
    sc_texture_destroy(&screen->tex);
error_destroy_renderer:
#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
    if (screen->gl_context) {
        SDL_GL_DestroyContext(screen->gl_context);
    }
#endif
    SDL_DestroyRenderer(screen->renderer);
error_destroy_window:
    SDL_DestroyWindow(screen->window);
error_destroy_fps_counter:
    sc_fps_counter_destroy(&screen->fps_counter);
error_destroy_frame_buffer:
    sc_frame_buffer_destroy(&screen->fb);

    return false;
}

static void
sc_screen_show_initial_window(struct sc_screen *screen) {
    int x = screen->req.x != SC_WINDOW_POSITION_UNDEFINED
          ? screen->req.x : (int) SDL_WINDOWPOS_CENTERED;
    int y = screen->req.y != SC_WINDOW_POSITION_UNDEFINED
          ? screen->req.y : (int) SDL_WINDOWPOS_CENTERED;
    struct sc_point position = {
        .x = x,
        .y = y,
    };

    struct sc_size window_size =
        get_initial_optimal_size(screen->content_size, screen->req.width,
                                                       screen->req.height);

    assert(is_windowed(screen));
    sc_sdl_set_window_size(screen->window, window_size);
    sc_sdl_set_window_position(screen->window, position);

    if (screen->req.fullscreen) {
        sc_screen_toggle_fullscreen(screen);
    }

    if (screen->req.start_fps_counter) {
        sc_fps_counter_start(&screen->fps_counter);
    }

    sc_sdl_show_window(screen->window);
    sc_screen_update_content_rect(screen);
}

void
sc_screen_hide_window(struct sc_screen *screen) {
    sc_sdl_hide_window(screen->window);
}

void
sc_screen_interrupt(struct sc_screen *screen) {
    sc_fps_counter_interrupt(&screen->fps_counter);
}

void
sc_screen_join(struct sc_screen *screen) {
    sc_fps_counter_join(&screen->fps_counter);
}

void
sc_screen_destroy(struct sc_screen *screen) {
#ifndef NDEBUG
    assert(!screen->open);
#endif
    sc_texture_destroy(&screen->tex);
    av_frame_free(&screen->frame);
#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
    SDL_GL_DestroyContext(screen->gl_context);
#endif
    SDL_DestroyRenderer(screen->renderer);
    SDL_DestroyWindow(screen->window);
    sc_fps_counter_destroy(&screen->fps_counter);
    sc_frame_buffer_destroy(&screen->fb);
}

static void
resize_for_content(struct sc_screen *screen, struct sc_size old_content_size,
                   struct sc_size new_content_size) {
    assert(screen->video);

    struct sc_size window_size = sc_sdl_get_window_size(screen->window);
    struct sc_size target_size = {
        .width = (uint32_t) window_size.width * new_content_size.width
                / old_content_size.width,
        .height = (uint32_t) window_size.height * new_content_size.height
                / old_content_size.height,
    };
    target_size = get_optimal_size(target_size, new_content_size, true);
    assert(is_windowed(screen));
    sc_sdl_set_window_size(screen->window, target_size);
}

static void
set_content_size(struct sc_screen *screen, struct sc_size new_content_size) {
    assert(screen->video);

    if (is_windowed(screen)) {
        resize_for_content(screen, screen->content_size, new_content_size);
    } else if (!screen->resize_pending) {
        // Store the windowed size to be able to compute the optimal size once
        // fullscreen/maximized/minimized are disabled
        screen->windowed_content_size = screen->content_size;
        screen->resize_pending = true;
    }

    screen->content_size = new_content_size;
}

static void
apply_pending_resize(struct sc_screen *screen) {
    assert(screen->video);

    assert(is_windowed(screen));
    if (screen->resize_pending) {
        resize_for_content(screen, screen->windowed_content_size,
                                   screen->content_size);
        screen->resize_pending = false;
    }
}

void
sc_screen_set_orientation(struct sc_screen *screen,
                          enum sc_orientation orientation) {
    assert(screen->video);

    if (orientation == screen->orientation) {
        return;
    }

    struct sc_size new_content_size =
        get_oriented_size(screen->frame_size, orientation);

    set_content_size(screen, new_content_size);

    screen->orientation = orientation;
    LOGI("Display orientation set to %s", sc_orientation_get_name(orientation));

    sc_screen_render(screen, true);
}

static bool
sc_screen_apply_frame(struct sc_screen *screen) {
    assert(screen->video);

    sc_fps_counter_add_rendered_frame(&screen->fps_counter);

    AVFrame *frame = screen->frame;
    struct sc_size new_frame_size = {frame->width, frame->height};

    if (!screen->has_frame
            || screen->frame_size.width != new_frame_size.width
            || screen->frame_size.height != new_frame_size.height) {

        // frame dimension changed
        screen->frame_size = new_frame_size;

        struct sc_size new_content_size =
            get_oriented_size(new_frame_size, screen->orientation);
        if (screen->has_frame) {
            set_content_size(screen, new_content_size);
            sc_screen_update_content_rect(screen);
        } else {
            // This is the first frame
            screen->has_frame = true;
            screen->content_size = new_content_size;
        }
    }

    bool ok = sc_texture_set_from_frame(&screen->tex, frame);
    if (!ok) {
        return false;
    }

    assert(screen->has_frame);
    if (!screen->has_video_window) {
        screen->has_video_window = true;
        // this is the very first frame, show the window
        sc_screen_show_initial_window(screen);

        if (sc_screen_is_relative_mode(screen)) {
            // Capture mouse on start
            sc_mouse_capture_set_active(&screen->mc, true);
        }
    }

    sc_screen_render(screen, false);
    return true;
}

static bool
sc_screen_update_frame(struct sc_screen *screen) {
    assert(screen->video);

    if (screen->paused) {
        if (!screen->resume_frame) {
            screen->resume_frame = av_frame_alloc();
            if (!screen->resume_frame) {
                LOG_OOM();
                return false;
            }
        } else {
            av_frame_unref(screen->resume_frame);
        }
        sc_frame_buffer_consume(&screen->fb, screen->resume_frame);
        return true;
    }

    av_frame_unref(screen->frame);
    sc_frame_buffer_consume(&screen->fb, screen->frame);
    return sc_screen_apply_frame(screen);
}

void
sc_screen_set_paused(struct sc_screen *screen, bool paused) {
    assert(screen->video);

    if (!paused && !screen->paused) {
        // nothing to do
        return;
    }

    if (screen->paused && screen->resume_frame) {
        // If display screen was paused, refresh the frame immediately, even if
        // the new state is also paused.
        av_frame_free(&screen->frame);
        screen->frame = screen->resume_frame;
        screen->resume_frame = NULL;
        bool ok = sc_screen_apply_frame(screen);
        if (!ok) {
            LOGE("Resume frame update failed");
        }
    }

    if (!paused) {
        LOGI("Display screen unpaused");
    } else if (!screen->paused) {
        LOGI("Display screen paused");
    } else {
        LOGI("Display screen re-paused");
    }

    screen->paused = paused;
}

void
sc_screen_toggle_fullscreen(struct sc_screen *screen) {
    assert(screen->video);

    bool req_fullscreen =
        !(SDL_GetWindowFlags(screen->window) & SDL_WINDOW_FULLSCREEN);

    bool ok = SDL_SetWindowFullscreen(screen->window, req_fullscreen);
    if (!ok) {
        LOGW("Could not switch fullscreen mode: %s", SDL_GetError());
        return;
    }

    LOGD("Requested %s mode", req_fullscreen ? "fullscreen" : "windowed");
}

void
sc_screen_resize_to_fit(struct sc_screen *screen) {
    assert(screen->video);

    if (!is_windowed(screen)) {
        return;
    }

    struct sc_point point = sc_sdl_get_window_position(screen->window);
    struct sc_size window_size = sc_sdl_get_window_size(screen->window);

    struct sc_size optimal_size =
        get_optimal_size(window_size, screen->content_size, false);

    // Center the window related to the device screen
    assert(optimal_size.width <= window_size.width);
    assert(optimal_size.height <= window_size.height);

    struct sc_point new_position = {
        .x = point.x + (window_size.width - optimal_size.width) / 2,
        .y = point.y + (window_size.height - optimal_size.height) / 2,
    };

    sc_sdl_set_window_size(screen->window, optimal_size);
    sc_sdl_set_window_position(screen->window, new_position);
    LOGD("Resized to optimal size: %ux%u", optimal_size.width,
                                           optimal_size.height);
}

void
sc_screen_resize_to_pixel_perfect(struct sc_screen *screen) {
    assert(screen->video);

    if (!is_windowed(screen)) {
        return;
    }

    struct sc_size content_size = screen->content_size;
    sc_sdl_set_window_size(screen->window, content_size);
    LOGD("Resized to pixel-perfect: %ux%u", content_size.width,
                                            content_size.height);
}

void
sc_screen_handle_event(struct sc_screen *screen, const SDL_Event *event) {
    // !video implies !has_video_window
    assert(screen->video || !screen->has_video_window);
    switch (event->type) {
        case SC_EVENT_NEW_FRAME: {
            bool ok = sc_screen_update_frame(screen);
            if (!ok) {
                LOGE("Frame update failed\n");
            }
            return;
        }
        case SDL_EVENT_WINDOW_EXPOSED:
            if (!screen->video || screen->has_video_window) {
                sc_screen_render(screen, true);
            }
            return;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (screen->has_video_window) {
                sc_screen_render(screen, true);
            }
            return;
        case SDL_EVENT_WINDOW_RESTORED:
            if (screen->has_video_window && is_windowed(screen)) {
                apply_pending_resize(screen);
                sc_screen_render(screen, true);
            }
            return;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            LOGD("Switched to fullscreen mode");
            assert(screen->has_video_window);
            return;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            LOGD("Switched to windowed mode");
            assert(screen->has_video_window);
            if (is_windowed(screen)) {
                apply_pending_resize(screen);
                sc_screen_render(screen, true);
            }
            return;
    }

    if (sc_screen_is_relative_mode(screen)
            && sc_mouse_capture_handle_event(&screen->mc, event)) {
        // The mouse capture handler consumed the event
        return;
    }

    sc_input_manager_handle_event(&screen->im, event);
}

struct sc_point
sc_screen_convert_drawable_to_frame_coords(struct sc_screen *screen,
                                           int32_t x, int32_t y) {
    assert(screen->video);

    enum sc_orientation orientation = screen->orientation;

    int32_t w = screen->content_size.width;
    int32_t h = screen->content_size.height;

    // screen->rect must be initialized to avoid a division by zero
    assert(screen->rect.w && screen->rect.h);

    x = (int64_t) (x - screen->rect.x) * w / screen->rect.w;
    y = (int64_t) (y - screen->rect.y) * h / screen->rect.h;

    struct sc_point result;
    switch (orientation) {
        case SC_ORIENTATION_0:
            result.x = x;
            result.y = y;
            break;
        case SC_ORIENTATION_90:
            result.x = y;
            result.y = w - x;
            break;
        case SC_ORIENTATION_180:
            result.x = w - x;
            result.y = h - y;
            break;
        case SC_ORIENTATION_270:
            result.x = h - y;
            result.y = x;
            break;
        case SC_ORIENTATION_FLIP_0:
            result.x = w - x;
            result.y = y;
            break;
        case SC_ORIENTATION_FLIP_90:
            result.x = h - y;
            result.y = w - x;
            break;
        case SC_ORIENTATION_FLIP_180:
            result.x = x;
            result.y = h - y;
            break;
        default:
            assert(orientation == SC_ORIENTATION_FLIP_270);
            result.x = y;
            result.y = x;
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

    struct sc_size window_size = sc_sdl_get_window_size(screen->window);
    int64_t ww = window_size.width;
    int64_t wh = window_size.height;

    struct sc_size drawable_size =
        sc_sdl_get_window_size_in_pixels(screen->window);
    int64_t dw = drawable_size.width;
    int64_t dh = drawable_size.height;

    // scale for HiDPI (64 bits for intermediate multiplications)
    *x = (int64_t) *x * dw / ww;
    *y = (int64_t) *y * dh / wh;
}
