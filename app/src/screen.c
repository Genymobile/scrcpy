#include "screen.h"

#include <string.h>
#include <SDL2/SDL.h>

#include "common.h"
#include "compat.h"
#include "icon.xpm"
#include "lock_util.h"
#include "log.h"
#include "tiny_xpm.h"
#include "video_buffer.h"

#define DISPLAY_MARGINS 96

// get the window size in a struct size
static struct size
get_native_window_size(SDL_Window *window) {
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
get_window_size(const struct screen *screen) {
    if (screen->fullscreen) {
        return screen->windowed_window_size;
    }
    return get_native_window_size(screen->window);
}

// set the window size to be applied when fullscreen is disabled
static void
set_window_size(struct screen *screen, struct size new_size) {
    // setting the window size during fullscreen is implementation defined,
    // so apply the resize only after fullscreen is disabled
    if (screen->fullscreen) {
        // SDL_SetWindowSize will be called when fullscreen will be disabled
        screen->windowed_window_size = new_size;
    } else {
        SDL_SetWindowSize(screen->window, new_size.width, new_size.height);
    }
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
get_optimal_size(struct size current_size, struct size frame_size) {
    if (frame_size.width == 0 || frame_size.height == 0) {
        // avoid division by 0
        return current_size;
    }

    struct size display_size;
    // 32 bits because we need to multiply two 16 bits values
    uint32_t w;
    uint32_t h;

    if (!get_preferred_display_bounds(&display_size)) {
        // cannot get display bounds, do not constraint the size
        w = current_size.width;
        h = current_size.height;
    } else {
        w = MIN(current_size.width, display_size.width);
        h = MIN(current_size.height, display_size.height);
    }

    bool keep_width = frame_size.width * h > frame_size.height * w;
    if (keep_width) {
        // remove black borders on top and bottom
        h = frame_size.height * w / frame_size.width;
    } else {
        // remove black borders on left and right (or none at all if it already
        // fits)
        w = frame_size.width * h / frame_size.height;
    }

    // w and h must fit into 16 bits
    SDL_assert_release(w < 0x10000 && h < 0x10000);
    return (struct size) {w, h};
}

// same as get_optimal_size(), but read the current size from the window
static inline struct size
get_optimal_window_size(const struct screen *screen, struct size frame_size) {
    struct size current_size = get_window_size(screen);
    return get_optimal_size(current_size, frame_size);
}

// initially, there is no current size, so use the frame size as current size
static inline struct size
get_initial_optimal_size(struct size frame_size) {
    return get_optimal_size(frame_size, frame_size);
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
screen_init_rendering(struct screen *screen, const char *device_name,
                      struct size frame_size, bool always_on_top) {
    screen->frame_size = frame_size;

    struct size window_size = get_initial_optimal_size(frame_size);
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

    screen->window = SDL_CreateWindow(device_name, SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED,
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

    if (SDL_RenderSetLogicalSize(screen->renderer, frame_size.width,
                                 frame_size.height)) {
        LOGE("Could not set renderer logical size: %s", SDL_GetError());
        screen_destroy(screen);
        return false;
    }

    SDL_Surface *icon = read_xpm(icon_xpm);
    if (!icon) {
        LOGE("Could not load icon: %s", SDL_GetError());
        screen_destroy(screen);
        return false;
    }
    SDL_SetWindowIcon(screen->window, icon);
    SDL_FreeSurface(icon);

    LOGI("Initial texture: %" PRIu16 "x%" PRIu16, frame_size.width,
                                                  frame_size.height);
    screen->texture = create_texture(screen->renderer, frame_size);
    if (!screen->texture) {
        LOGC("Could not create texture: %s", SDL_GetError());
        screen_destroy(screen);
        return false;
    }

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

// recreate the texture and resize the window if the frame size has changed
static bool
prepare_for_frame(struct screen *screen, struct size new_frame_size) {
    if (screen->frame_size.width != new_frame_size.width
            || screen->frame_size.height != new_frame_size.height) {
        if (SDL_RenderSetLogicalSize(screen->renderer, new_frame_size.width,
                                     new_frame_size.height)) {
            LOGE("Could not set renderer logical size: %s", SDL_GetError());
            return false;
        }

        // frame dimension changed, destroy texture
        SDL_DestroyTexture(screen->texture);

        struct size current_size = get_window_size(screen);
        struct size target_size = {
            (uint32_t) current_size.width * new_frame_size.width
                    / screen->frame_size.width,
            (uint32_t) current_size.height * new_frame_size.height
                    / screen->frame_size.height,
        };
        target_size = get_optimal_size(target_size, new_frame_size);
        set_window_size(screen, target_size);

        screen->frame_size = new_frame_size;

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
    SDL_RenderCopy(screen->renderer, screen->texture, NULL, NULL);
    SDL_RenderPresent(screen->renderer);
}

void
screen_switch_fullscreen(struct screen *screen) {
    if (!screen->fullscreen) {
        // going to fullscreen, store the current windowed window size
        screen->windowed_window_size = get_native_window_size(screen->window);
    }
    uint32_t new_mode = screen->fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (SDL_SetWindowFullscreen(screen->window, new_mode)) {
        LOGW("Could not switch fullscreen mode: %s", SDL_GetError());
        return;
    }

    screen->fullscreen = !screen->fullscreen;
    if (!screen->fullscreen) {
        // fullscreen disabled, restore expected windowed window size
        SDL_SetWindowSize(screen->window, screen->windowed_window_size.width,
                          screen->windowed_window_size.height);
    }

    LOGD("Switched to %s mode", screen->fullscreen ? "fullscreen" : "windowed");
    screen_render(screen);
}

void
screen_resize_to_fit(struct screen *screen) {
    if (!screen->fullscreen) {
        struct size optimal_size = get_optimal_window_size(screen,
                                                           screen->frame_size);
        SDL_SetWindowSize(screen->window, optimal_size.width,
                          optimal_size.height);
        LOGD("Resized to optimal size");
    }
}

void
screen_resize_to_pixel_perfect(struct screen *screen) {
    if (!screen->fullscreen) {
        SDL_SetWindowSize(screen->window, screen->frame_size.width,
                          screen->frame_size.height);
        LOGD("Resized to pixel-perfect");
    }
}
