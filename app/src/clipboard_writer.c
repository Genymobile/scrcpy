#include "clipboard_writer.h"

#include <string.h>
#include <SDL3/SDL.h>

#include "common.h"

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
sc_clipboard_write_png(void *data, size_t size) {
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        SDL_free(data);
        return SDL_SetError("Video subsystem is not initialized");
    }

    struct sc_clipboard_png *png = SDL_malloc(sizeof(*png));
    if (!png) {
        SDL_free(data);
        return SDL_SetError("Could not allocate PNG clipboard data");
    }
    png->data = data;
    png->size = size;

    static const char *const mime_types[] = {"image/png"};
    bool ok = SDL_SetClipboardData(sc_clipboard_get_png,
                                   sc_clipboard_cleanup_png, png,
                                   mime_types, ARRAY_LEN(mime_types));
    // With valid parameters and an initialized video subsystem, SDL stores the
    // cleanup callback before invoking the platform backend. It therefore owns
    // png after this call even when the backend reports an error.
    return ok;
}
