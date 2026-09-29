#include "clipboard.h"

#include <string.h>

#include "util/log.h"

struct sc_clipboard_png {
    void *data;
    size_t size;
};

static const void * SDLCALL
sc_clipboard_get_png(void *userdata, const char *mime_type, size_t *size) {
    struct sc_clipboard_png *png = userdata;
    if (!mime_type || strcmp(mime_type, "image/png")) {
        *size = 0;
        return NULL;
    }

    *size = png->size;
    return png->data;
}

static void SDLCALL
sc_clipboard_cleanup_png(void *userdata) {
    struct sc_clipboard_png *png = userdata;
    SDL_free(png->data);
    SDL_free(png);
}

bool
sc_clipboard_set_png(SDL_Surface *surface) {
    SDL_IOStream *io = SDL_IOFromDynamicMem();
    if (!io) {
        LOGE("Could not create PNG memory stream: %s", SDL_GetError());
        return false;
    }

    if (!SDL_SavePNG_IO(surface, io, false)) {
        LOGE("Could not encode screenshot as PNG: %s", SDL_GetError());
        SDL_CloseIO(io);
        return false;
    }

    Sint64 png_size = SDL_GetIOSize(io);
    SDL_PropertiesID props = SDL_GetIOProperties(io);
    void *png_data = props
        ? SDL_GetPointerProperty(props,
                                 SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER,
                                 NULL)
        : NULL;
    if (png_size <= 0 || !png_data) {
        LOGE("Could not retrieve encoded PNG: %s", SDL_GetError());
        SDL_CloseIO(io);
        return false;
    }

    bool ok = SDL_SetPointerProperty(
        props, SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
    SDL_CloseIO(io);
    if (!ok) {
        LOGE("Could not take ownership of encoded PNG: %s", SDL_GetError());
        return false;
    }

    struct sc_clipboard_png *png = SDL_malloc(sizeof(*png));
    if (!png) {
        SDL_free(png_data);
        LOG_OOM();
        return false;
    }
    png->data = png_data;
    png->size = png_size;

    static const char *const mime_types[] = {"image/png"};
    ok = SDL_SetClipboardData(sc_clipboard_get_png,
                              sc_clipboard_cleanup_png, png,
                              mime_types, ARRAY_LEN(mime_types));
    if (!ok) {
        LOGE("Could not copy screenshot to clipboard: %s", SDL_GetError());
        sc_clipboard_cleanup_png(png);
        return false;
    }
    return true;
}
