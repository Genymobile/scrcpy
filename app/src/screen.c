#include "screen.h"

#include <assert.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "config.h"
#include "common.h"
#include "compat.h"
#include "icon.xpm"
#include "tiny_xpm.h"
#include "video_buffer.h"
#include "util/lock.h"
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
get_window_size(SDL_Window *window) {
    int width;
    int height;
    SDL_GetWindowSize(window, &width, &height);

    struct size size;
    size.width = width;
    size.height = height;
    return size;
}

// get the windowed window size
static struct size
get_windowed_window_size(const struct screen *screen) {
    if (screen->fullscreen || screen->maximized) {
        return screen->windowed_window_size;
    }
    return get_window_size(screen->window);
}

// apply the windowed window size if fullscreen and maximized are disabled
static void
apply_windowed_size(struct screen *screen) {
    if (!screen->fullscreen && !screen->maximized) {
        SDL_SetWindowSize(screen->window, screen->windowed_window_size.width,
                                          screen->windowed_window_size.height);
    }
}

// set the window size to be applied when fullscreen is disabled
static void
set_window_size(struct screen *screen, struct size new_size) {
    // setting the window size during fullscreen is implementation defined,
    // so apply the resize only after fullscreen is disabled
    screen->windowed_window_size = new_size;
    apply_windowed_size(screen);
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

    struct size display_size;
    // 32 bits because we need to multiply two 16 bits values
    uint32_t w;
    uint32_t h;

    if (!get_preferred_display_bounds(&display_size)) {
        // could not get display bounds, do not constraint the size
        w = current_size.width;
        h = current_size.height;
    } else {
        w = MIN(current_size.width, display_size.width);
        h = MIN(current_size.height, display_size.height);
    }

    bool keep_width = content_size.width * h > content_size.height * w;
    if (keep_width) {
        // remove black borders on top and bottom
        h = content_size.height * w / content_size.width;
    } else {
        // remove black borders on left and right (or none at all if it already
        // fits)
        w = content_size.width * h / content_size.height;
    }

    // w and h must fit into 16 bits
    assert(w < 0x10000 && h < 0x10000);
    return (struct size) {w, h};
}

// same as get_optimal_size(), but read the current size from the window
static inline struct size
get_optimal_window_size(const struct screen *screen, struct size content_size) {
    struct size windowed_size = get_windowed_window_size(screen);
    return get_optimal_size(windowed_size, content_size);
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

void
screen_init(struct screen *screen) {
    *screen = (struct screen) SCREEN_INITIALIZER;
}

static inline SDL_Texture *
create_texture(SDL_Renderer *renderer, struct size frame_size) {
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                             SDL_TEXTUREACCESS_STREAMING,
                             frame_size.width, frame_size.height);
}

bool
screen_init_rendering(struct screen *screen, const char *window_title,
                      struct size frame_size, bool always_on_top,
                      int16_t window_x, int16_t window_y, uint16_t window_width,
                      uint16_t window_height, bool window_borderless,
                      uint8_t rotation) {
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

    int x = window_x != WINDOW_POSITION_UNDEFINED
          ? window_x : (int) SDL_WINDOWPOS_UNDEFINED;
    int y = window_y != WINDOW_POSITION_UNDEFINED
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

    if (SDL_RenderSetLogicalSize(screen->renderer, content_size.width,
                                 content_size.height)) {
        LOGE("Could not set renderer logical size: %s", SDL_GetError());
        screen_destroy(screen);
        return false;
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
    screen->texture = create_texture(screen->renderer, frame_size);
    if (!screen->texture) {
        LOGC("Could not create texture: %s", SDL_GetError());
        screen_destroy(screen);
        return false;
    }

    screen->windowed_window_size = window_size;

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

void
screen_set_rotation(struct screen *screen, unsigned rotation) {
    assert(rotation < 4);
    if (rotation == screen->rotation) {
        return;
    }

    struct size old_content_size = screen->content_size;
    struct size new_content_size =
        get_rotated_size(screen->frame_size, rotation);

    if (SDL_RenderSetLogicalSize(screen->renderer,
                                 new_content_size.width,
                                 new_content_size.height)) {
        LOGE("Could not set renderer logical size: %s", SDL_GetError());
        return;
    }

    struct size windowed_size = get_windowed_window_size(screen);
    struct size target_size = {
        .width = (uint32_t) windowed_size.width * new_content_size.width
                / old_content_size.width,
        .height = (uint32_t) windowed_size.height * new_content_size.height
                / old_content_size.height,
    };
    target_size = get_optimal_size(target_size, new_content_size);
    set_window_size(screen, target_size);

    screen->content_size = new_content_size;
    screen->rotation = rotation;
    LOGI("Display rotation set to %u", rotation);

    screen_render(screen);
}

// recreate the texture and resize the window if the frame size has changed
static bool
prepare_for_frame(struct screen *screen, struct size new_frame_size) {
    if (screen->frame_size.width != new_frame_size.width
            || screen->frame_size.height != new_frame_size.height) {
        struct size new_content_size =
            get_rotated_size(new_frame_size, screen->rotation);
        if (SDL_RenderSetLogicalSize(screen->renderer,
                                     new_content_size.width,
                                     new_content_size.height)) {
            LOGE("Could not set renderer logical size: %s", SDL_GetError());
            return false;
        }

        // frame dimension changed, destroy texture
        SDL_DestroyTexture(screen->texture);

        struct size content_size = screen->content_size;
        struct size windowed_size = get_windowed_window_size(screen);
        struct size target_size = {
            (uint32_t) windowed_size.width * new_content_size.width
                    / content_size.width,
            (uint32_t) windowed_size.height * new_content_size.height
                    / content_size.height,
        };
        target_size = get_optimal_size(target_size, new_content_size);
        set_window_size(screen, target_size);

        screen->frame_size = new_frame_size;
        screen->content_size = new_content_size;

        LOGI("New texture: %" PRIu16 "x%" PRIu16,
                     screen->frame_size.width, screen->frame_size.height);
        screen->texture = create_texture(screen->renderer, new_frame_size);
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
}

bool
screen_update_frame(struct screen *screen, struct video_buffer *vb) {
    mutex_lock(vb->mutex);
    const AVFrame *frame = video_buffer_consume_rendered_frame(vb);
    struct size new_frame_size = {frame->width, frame->height};
    if (!prepare_for_frame(screen, new_frame_size)) {
        mutex_unlock(vb->mutex);
        return false;
    }
    update_texture(screen, frame);
    mutex_unlock(vb->mutex);

    screen_render(screen);
    return true;
}

void
screen_render(struct screen *screen) {
    SDL_RenderClear(screen->renderer);
    if (screen->rotation == 0) {
        SDL_RenderCopy(screen->renderer, screen->texture, NULL, NULL);
    } else {
        // rotation in RenderCopyEx() is clockwise, while screen->rotation is
        // counterclockwise (to be consistent with --lock-video-orientation)
        int cw_rotation = (4 - screen->rotation) % 4;
        double angle = 90 * cw_rotation;

        SDL_Rect *dstrect = NULL;
        SDL_Rect rect;
        if (screen->rotation & 1) {
            struct size size = screen->content_size;
            rect.x = (size.width - size.height) / 2;
            rect.y = (size.height - size.width) / 2;
            rect.w = size.height;
            rect.h = size.width;
            dstrect = &rect;
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
    apply_windowed_size(screen);

    LOGD("Switched to %s mode", screen->fullscreen ? "fullscreen" : "windowed");
    screen_render(screen);
}

void
screen_resize_to_fit(struct screen *screen) {
    if (screen->fullscreen) {
        return;
    }

    if (screen->maximized) {
        SDL_RestoreWindow(screen->window);
        screen->maximized = false;
    }

    struct size optimal_size =
        get_optimal_window_size(screen, screen->content_size);
    SDL_SetWindowSize(screen->window, optimal_size.width, optimal_size.height);
    LOGD("Resized to optimal size");
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
    LOGD("Resized to pixel-perfect");
}

void
screen_handle_window_event(struct screen *screen,
                           const SDL_WindowEvent *event) {
    switch (event->event) {
        case SDL_WINDOWEVENT_EXPOSED:
            screen_render(screen);
            break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            if (!screen->fullscreen && !screen->maximized) {
                // Backup the previous size: if we receive the MAXIMIZED event,
                // then the new size must be ignored (it's the maximized size).
                // We could not rely on the window flags due to race conditions
                // (they could be updated asynchronously, at least on X11).
                screen->windowed_window_size_backup =
                    screen->windowed_window_size;

                // Save the windowed size, so that it is available once the
                // window is maximized or fullscreen is enabled.
                screen->windowed_window_size = get_window_size(screen->window);
            }
            screen_render(screen);
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
            // The backup size must be non-nul.
            assert(screen->windowed_window_size_backup.width);
            assert(screen->windowed_window_size_backup.height);
            // Revert the last size, it was updated while screen was maximized.
            screen->windowed_window_size = screen->windowed_window_size_backup;
#ifdef DEBUG
            // Reset the backup to invalid values to detect unexpected usage
            screen->windowed_window_size_backup.width = 0;
            screen->windowed_window_size_backup.height = 0;
#endif
            screen->maximized = true;
            break;
        case SDL_WINDOWEVENT_RESTORED:
            screen->maximized = false;
            apply_windowed_size(screen);
            break;
    }
}
