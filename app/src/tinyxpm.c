#include "tinyxpm.h"

#include <stdio.h>
#include <stdlib.h>

#include "log.h"

struct index {
    char c;
    Uint32 color;
};

static SDL_bool find_color(struct index *index, int len, char c, Uint32 *color) {
    // there are typically very few color, so it's ok to iterate over the array
    for (int i = 0; i < len; ++i) {
        if (index[i].c == c) {
            *color = index[i].color;
            return SDL_TRUE;
        }
    }
    *color = 0;
    return SDL_FALSE;
}

// We encounter some problems with SDL2_image on MSYS2 (Windows),
// so here is our own XPM parsing not to depend on SDL_image.
//
// We do not hardcode the binary image to keep some flexibility to replace the
// icon easily (just by replacing icon.xpm).
//
// Parameter is not "const char *" because XPM formats are generally stored in a
// (non-const) "char *"
SDL_Surface *read_xpm(char *xpm[]) {
#if SDL_ASSERT_LEVEL >= 2
    // patch the XPM to change the icon color in debug mode
    xpm[2] = ".	c #CC00CC";
#endif

    char *endptr;
    // *** No error handling, assume the XPM source is valid ***
    // (it's in our source repo)
    // Assertions are only checked in debug
    Uint32 width = strtol(xpm[0], &endptr, 10);
    Uint32 height = strtol(endptr + 1, &endptr, 10);
    Uint32 colors = strtol(endptr + 1, &endptr, 10);
    Uint32 chars = strtol(endptr + 1, &endptr, 10);

    // sanity checks
    SDL_assert(width < 256);
    SDL_assert(height < 256);
    SDL_assert(colors < 256);
    SDL_assert(chars == 1); // this implementation does not support more

    // init index
    struct index index[colors];
    for (int i = 0; i < colors; ++i) {
        const char *line = xpm[1+i];
        index[i].c = line[0];
        SDL_assert(line[1] == '\t');
        SDL_assert(line[2] == 'c');
        SDL_assert(line[3] == ' ');
        if (line[4] == '#') {
            index[i].color = 0xff000000 | strtol(&line[5], &endptr, 0x10);
            SDL_assert(*endptr == '\0');
        } else {
            SDL_assert(!strcmp("None", &line[4]));
            index[i].color = 0;
        }
    }

    // parse image
    Uint32 *pixels = SDL_malloc(4 * width * height);
    if (!pixels) {
        LOGE("Could not allocate icon memory");
        return NULL;
    }
    for (int y = 0; y < height; ++y) {
        const char *line = xpm[1 + colors + y];
        for (int x = 0; x < width; ++x) {
            char c = line[x];
            Uint32 color;
            SDL_bool color_found = find_color(index, colors, c, &color);
            SDL_assert(color_found);
            pixels[y * width + x] = color;
        }
    }

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    Uint32 amask = 0x000000ff;
    Uint32 rmask = 0x0000ff00;
    Uint32 gmask = 0x00ff0000;
    Uint32 bmask = 0xff000000;
#else // little endian, like x86
    Uint32 amask = 0xff000000;
    Uint32 rmask = 0x00ff0000;
    Uint32 gmask = 0x0000ff00;
    Uint32 bmask = 0x000000ff;
#endif

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels,
                                                    width, height,
                                                    32, 4 * width,
                                                    rmask, gmask, bmask, amask);
    // make the surface own the raw pixels
    surface->flags &= ~SDL_PREALLOC;
    return surface;
}
