#include "sdl.h"

#include <assert.h>
#include <stdlib.h>

#include "util/log.h"

SDL_Window *
sc_sdl_create_window(const char *title, int64_t x, int64_t y, int64_t width,
                     int64_t height, int64_t flags) {
    SDL_Window *window = NULL;

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return NULL;
    }

    bool ok =
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                              title);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
                                width);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                                height);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                                flags);

    if (!ok) {
        SDL_DestroyProperties(props);
        return NULL;
    }

    window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    return window;
}

struct sc_size
sc_sdl_get_window_size(SDL_Window *window) {
    int width;
    int height;
    bool ok = SDL_GetWindowSize(window, &width, &height);
    if (!ok) {
        LOGE("Could not get window size: %s", SDL_GetError());
        LOGE("Please report the error");
        // fatal error
        abort();
    }

    struct sc_size size = {
        .width = width,
        .height = height,
    };
    return size;
}

struct sc_size
sc_sdl_get_window_size_in_pixels(SDL_Window *window) {
    int width;
    int height;
    bool ok = SDL_GetWindowSizeInPixels(window, &width, &height);
    if (!ok) {
        LOGE("Could not get window size: %s", SDL_GetError());
        LOGE("Please report the error");
        // fatal error
        abort();
    }

    struct sc_size size = {
        .width = width,
        .height = height,
    };
    return size;
}

void
sc_sdl_set_window_size(SDL_Window *window, struct sc_size size) {
    bool ok = SDL_SetWindowSize(window, size.width, size.height);
    if (!ok) {
        LOGE("Could not set window size: %s", SDL_GetError());
        assert(!"unexpected");
    }
}

void
sc_sdl_set_window_aspect_ratio(SDL_Window *window, float min_aspect, float max_aspect) {
    bool ok = SDL_SetWindowAspectRatio(window, min_aspect, max_aspect);
    if (!ok) {
        LOGE("Could not set window aspect ratio: %s", SDL_GetError());
        assert(!"unexpected");
    }
}

struct sc_point
sc_sdl_get_window_position(SDL_Window *window) {
    int x;
    int y;
    bool ok = SDL_GetWindowPosition(window, &x, &y);
    if (!ok) {
        LOGE("Could not get window position: %s", SDL_GetError());
        LOGE("Please report the error");
        // fatal error
        abort();
    }

    struct sc_point point = {
        .x = x,
        .y = y,
    };
    return point;
}

void
sc_sdl_set_window_position(SDL_Window *window, struct sc_point point) {
    bool ok = SDL_SetWindowPosition(window, point.x, point.y);
    if (!ok) {
        LOGE("Could not set window position: %s", SDL_GetError());
        assert(!"unexpected");
    }
}

void
sc_sdl_show_window(SDL_Window *window) {
    bool ok = SDL_ShowWindow(window);
    if (!ok) {
        LOGE("Could not show window: %s", SDL_GetError());
        assert(!"unexpected");
    }
}

void
sc_sdl_hide_window(SDL_Window *window) {
    bool ok = SDL_HideWindow(window);
    if (!ok) {
        LOGE("Could not hide window: %s", SDL_GetError());
        assert(!"unexpected");
    }
}

struct sc_size
sc_sdl_get_render_output_size(SDL_Renderer *renderer) {
    int width;
    int height;
    bool ok = SDL_GetRenderOutputSize(renderer, &width, &height);
    if (!ok) {
        LOGE("Could not get render output size: %s", SDL_GetError());
        LOGE("Please report the error");
        // fatal error
        abort();
    }

    struct sc_size size = {
        .width = width,
        .height = height,
    };
    return size;
}

bool
sc_sdl_render_clear(SDL_Renderer *renderer) {
    bool ok = SDL_RenderClear(renderer);
    if (!ok) {
        LOGW("Could not clear rendering: %s", SDL_GetError());
    }
    return ok;
}

void
sc_sdl_render_present(SDL_Renderer *renderer) {
    bool ok = SDL_RenderPresent(renderer);
    if (!ok) {
        LOGE("Could not render: %s", SDL_GetError());
        assert(!"unexpected");
    }
}
