#include "screen.h"

#include <assert.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "icon.xpm"
#include "scrcpy.h"
#include "tiny_xpm.h"
#include "video_buffer.h"
#include "util/log.h"

#define DISPLAY_MARGINS 96

static inline struct size
get_rotated_size(struct size size, int rotation) {
    struct size rotated_size;
    if (rotation & 1) {
        rotated_size.width = size.height;
        rotated_size.height = size.width;
    } else {
        rotated_size.width = size.width;
        rotated_size.height = size.height;
    }
    return rotated_size;
}

// get the window size in a struct size
static struct size
get_window_size(const struct screen *screen) {
    int width;
    int height;
    SDL_GetWindowSize(screen->window, &width, &height);

    struct size size;
    size.width = width;
    size.height = height;
    return size;
}

// set the window size to be applied when fullscreen is disabled
static void
set_window_size(struct screen *screen, struct size new_size) {
    assert(!screen->fullscreen);
    assert(!screen->maximized);
    SDL_SetWindowSize(screen->window, new_size.width, new_size.height);
}

// get the preferred display bounds (i.e. the screen bounds with some margins)
static bool
get_preferred_display_bounds(struct size *bounds) {
    SDL_Rect rect;
#ifdef SCRCPY_SDL_HAS_GET_DISPLAY_USABLE_BOUNDS
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayUsableBounds((i), (r))
#else
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayBounds((i), (r))
#endif
    if (GET_DISPLAY_BOUNDS(0, &rect)) {
        LOGW("Could not get display usable bounds: %s", SDL_GetError());
        return false;
    }

    bounds->width = MAX(0, rect.w - DISPLAY_MARGINS);
    bounds->height = MAX(0, rect.h - DISPLAY_MARGINS);
    return true;
}

static bool
is_optimal_size(struct size current_size, struct size content_size) {
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
static struct size
get_optimal_size(struct size current_size, struct size content_size) {
    if (content_size.width == 0 || content_size.height == 0) {
        // avoid division by 0
        return current_size;
    }

    struct size window_size;

    struct size display_size;
    if (!get_preferred_display_bounds(&display_size)) {
        // could not get display bounds, do not constraint the size
        window_size.width = current_size.width;
        window_size.height = current_size.height;
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

// same as get_optimal_size(), but read the current size from the window
static inline struct size
get_optimal_window_size(const struct screen *screen, struct size content_size) {
    struct size window_size = get_window_size(screen);
    return get_optimal_size(window_size, content_size);
}

// initially, there is no current size, so use the frame size as current size
// req_width and req_height, if not 0, are the sizes requested by the user
static inline struct size
get_initial_optimal_size(struct size content_size, uint16_t req_width,
                         uint16_t req_height) {
    struct size window_size;
    if (!req_width && !req_height) {
        window_size = get_optimal_size(content_size, content_size);
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

static void
screen_update_content_rect(struct screen *screen) {
    int dw;
    int dh;
    SDL_GL_GetDrawableSize(screen->window, &dw, &dh);

    struct size content_size = screen->content_size;
    // The drawable size is the window size * the HiDPI scale
    struct size drawable_size = {dw, dh};

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

void
screen_init(struct screen *screen) {
    *screen = (struct screen) SCREEN_INITIALIZER;
}

static inline SDL_Texture *
create_texture(struct screen *screen) {
    SDL_Renderer *renderer = screen->renderer;
    struct size size = screen->frame_size;
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

bool
screen_init_rendering(struct screen *screen, const char *window_title,
                      struct size frame_size, bool always_on_top,
                      int16_t window_x, int16_t window_y, uint16_t window_width,
                      uint16_t window_height, bool window_borderless,
                      uint8_t rotation, bool mipmaps) {
    screen->frame_size = frame_size;
    screen->rotation = rotation;
    if (rotation) {
        LOGI("Initial display rotation set to %u", rotation);
    }
    struct size content_size = get_rotated_size(frame_size, screen->rotation);
    screen->content_size = content_size;

    struct size window_size =
        get_initial_optimal_size(content_size, window_width, window_height);
    uint32_t window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
#ifdef HIDPI_SUPPORT
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    if (always_on_top) {
#ifdef SCRCPY_SDL_HAS_WINDOW_ALWAYS_ON_TOP
        window_flags |= SDL_WINDOW_ALWAYS_ON_TOP;
#else
        LOGW("The 'always on top' flag is not available "
             "(compile with SDL >= 2.0.5 to enable it)");
#endif
    }
    if (window_borderless) {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    int x = window_x != SC_WINDOW_POSITION_UNDEFINED
          ? window_x : (int) SDL_WINDOWPOS_UNDEFINED;
    int y = window_y != SC_WINDOW_POSITION_UNDEFINED
          ? window_y : (int) SDL_WINDOWPOS_UNDEFINED;
    screen->window = SDL_CreateWindow(window_title, x, y,
                                      window_size.width, window_size.height,
                                      window_flags);
    if (!screen->window) {
        LOGC("Could not create window: %s", SDL_GetError());
        return false;
    }

    screen->renderer = SDL_CreateRenderer(screen->window, -1,
                                          SDL_RENDERER_ACCELERATED);
    if (!screen->renderer) {
        LOGC("Could not create renderer: %s", SDL_GetError());
        screen_destroy(screen);
        return false;
    }

    SDL_RendererInfo renderer_info;
    int r = SDL_GetRendererInfo(screen->renderer, &renderer_info);
    const char *renderer_name = r ? NULL : renderer_info.name;
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    // starts with "opengl"
    screen->use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (screen->use_opengl) {
        struct sc_opengl *gl = &screen->gl;
        sc_opengl_init(gl);

        LOGI("OpenGL version: %s", gl->version);

        if (mipmaps) {
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
    } else {
        LOGD("Trilinear filtering disabled (not an OpenGL renderer)");
    }

    SDL_Surface *icon = read_xpm(icon_xpm);
    if (icon) {
        SDL_SetWindowIcon(screen->window, icon);
        SDL_FreeSurface(icon);
    } else {
        LOGW("Could not load icon");
    }

    LOGI("Initial texture: %" PRIu16 "x%" PRIu16, frame_size.width,
                                                  frame_size.height);
    screen->texture = create_texture(screen);
    if (!screen->texture) {
        LOGC("Could not create texture: %s", SDL_GetError());
        screen_destroy(screen);
        return false;
    }

    // Reset the window size to trigger a SIZE_CHANGED event, to workaround
    // HiDPI issues with some SDL renderers when several displays having
    // different HiDPI scaling are connected
    SDL_SetWindowSize(screen->window, window_size.width, window_size.height);

    screen_update_content_rect(screen);

    return true;
}

void
screen_show_window(struct screen *screen) {
    SDL_ShowWindow(screen->window);
}

void
screen_destroy(struct screen *screen) {
    if (screen->texture) {
        SDL_DestroyTexture(screen->texture);
    }
    if (screen->renderer) {
        SDL_DestroyRenderer(screen->renderer);
    }
    if (screen->window) {
        SDL_DestroyWindow(screen->window);
    }
}

static void
resize_for_content(struct screen *screen, struct size old_content_size,
                                                 struct size new_content_size) {
    struct size window_size = get_window_size(screen);
    struct size target_size = {
        .width = (uint32_t) window_size.width * new_content_size.width
                / old_content_size.width,
        .height = (uint32_t) window_size.height * new_content_size.height
                / old_content_size.height,
    };
    target_size = get_optimal_size(target_size, new_content_size);
    set_window_size(screen, target_size);
}

static void
set_content_size(struct screen *screen, struct size new_content_size) {
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
apply_pending_resize(struct screen *screen) {
    assert(!screen->fullscreen);
    assert(!screen->maximized);
    if (screen->resize_pending) {
        resize_for_content(screen, screen->windowed_content_size,
                                   screen->content_size);
        screen->resize_pending = false;
    }
}

void
screen_set_rotation(struct screen *screen, unsigned rotation) {
    assert(rotation < 4);
    if (rotation == screen->rotation) {
        return;
    }

    struct size new_content_size =
        get_rotated_size(screen->frame_size, rotation);

    set_content_size(screen, new_content_size);

    screen->rotation = rotation;
    LOGI("Display rotation set to %u", rotation);

    screen_render(screen, true);
}

// recreate the texture and resize the window if the frame size has changed
static bool
prepare_for_frame(struct screen *screen, struct size new_frame_size) {
    if (screen->frame_size.width != new_frame_size.width
            || screen->frame_size.height != new_frame_size.height) {
        // frame dimension changed, destroy texture
        SDL_DestroyTexture(screen->texture);

        screen->frame_size = new_frame_size;

        struct size new_content_size =
            get_rotated_size(new_frame_size, screen->rotation);
        set_content_size(screen, new_content_size);

        screen_update_content_rect(screen);

        LOGI("New texture: %" PRIu16 "x%" PRIu16,
                     screen->frame_size.width, screen->frame_size.height);
        screen->texture = create_texture(screen);
        if (!screen->texture) {
            LOGC("Could not create texture: %s", SDL_GetError());
            return false;
        }
    }

    return true;
}

// write the frame into the texture
static void
update_texture(struct screen *screen, const AVFrame *frame) {
    SDL_UpdateYUVTexture(screen->texture, NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);

    if (screen->mipmaps) {
        assert(screen->use_opengl);
        SDL_GL_BindTexture(screen->texture, NULL, NULL);
        screen->gl.GenerateMipmap(GL_TEXTURE_2D);
        SDL_GL_UnbindTexture(screen->texture);
    }
}

bool
screen_update_frame(struct screen *screen, struct video_buffer *vb) {
    const AVFrame *frame = video_buffer_take_rendering_frame(vb);
    struct size new_frame_size = {frame->width, frame->height};
    if (!prepare_for_frame(screen, new_frame_size)) {
        sc_mutex_unlock(&vb->mutex);
        return false;
    }
    update_texture(screen, frame);

    screen_render(screen, false);
    return true;
}

void
screen_render(struct screen *screen, bool update_content_rect) {
    if (update_content_rect) {
        screen_update_content_rect(screen);
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

void
screen_switch_fullscreen(struct screen *screen) {
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
    screen_render(screen, true);
}

void
screen_resize_to_fit(struct screen *screen) {
    if (screen->fullscreen || screen->maximized) {
        return;
    }

    struct size optimal_size =
        get_optimal_window_size(screen, screen->content_size);
    SDL_SetWindowSize(screen->window, optimal_size.width, optimal_size.height);
    LOGD("Resized to optimal size: %ux%u", optimal_size.width,
                                           optimal_size.height);
}

void
screen_resize_to_pixel_perfect(struct screen *screen) {
    if (screen->fullscreen) {
        return;
    }

    if (screen->maximized) {
        SDL_RestoreWindow(screen->window);
        screen->maximized = false;
    }

    struct size content_size = screen->content_size;
    SDL_SetWindowSize(screen->window, content_size.width, content_size.height);
    LOGD("Resized to pixel-perfect: %ux%u", content_size.width,
                                            content_size.height);
}

void
screen_handle_window_event(struct screen *screen,
                           const SDL_WindowEvent *event) {
    switch (event->event) {
        case SDL_WINDOWEVENT_EXPOSED:
            screen_render(screen, true);
            break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            screen_render(screen, true);
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
            screen->maximized = true;
            break;
        case SDL_WINDOWEVENT_RESTORED:
            if (screen->fullscreen) {
                // On Windows, in maximized+fullscreen, disabling fullscreen
                // mode unexpectedly triggers the "restored" then "maximized"
                // events, leaving the window in a weird state (maximized
                // according to the events, but not maximized visually).
                break;
            }
            screen->maximized = false;
            apply_pending_resize(screen);
            break;
    }
}

struct point
screen_convert_drawable_to_frame_coords(struct screen *screen,
                                        int32_t x, int32_t y) {
    unsigned rotation = screen->rotation;
    assert(rotation < 4);

    int32_t w = screen->content_size.width;
    int32_t h = screen->content_size.height;


    x = (int64_t) (x - screen->rect.x) * w / screen->rect.w;
    y = (int64_t) (y - screen->rect.y) * h / screen->rect.h;

    // rotate
    struct point result;
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

struct point
screen_convert_window_to_frame_coords(struct screen *screen,
                                      int32_t x, int32_t y) {
    screen_hidpi_scale_coords(screen, &x, &y);
    return screen_convert_drawable_to_frame_coords(screen, x, y);
}

void
screen_hidpi_scale_coords(struct screen *screen, int32_t *x, int32_t *y) {
    // take the HiDPI scaling (dw/ww and dh/wh) into account
    int ww, wh, dw, dh;
    SDL_GetWindowSize(screen->window, &ww, &wh);
    SDL_GL_GetDrawableSize(screen->window, &dw, &dh);

    // scale for HiDPI (64 bits for intermediate multiplications)
    *x = (int64_t) *x * dw / ww;
    *y = (int64_t) *y * dh / wh;
}
