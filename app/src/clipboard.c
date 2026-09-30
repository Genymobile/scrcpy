#include "clipboard.h"

#include "clipboard_writer.h"
#include "util/log.h"

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

    ok = sc_clipboard_write_png(png_data, (size_t) png_size);
    if (!ok) {
        LOGE("Could not copy screenshot to clipboard: %s", SDL_GetError());
        return false;
    }
    return true;
}
