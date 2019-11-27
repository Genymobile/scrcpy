#include "tiny_xpm.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "util/log.h"

struct index {
    char c;
    uint32_t color;
};

static bool
find_color(struct index *index, int len, char c, uint32_t *color) {
    // there are typically very few color, so it's ok to iterate over the array
    for (int i = 0; i < len; ++i) {
        if (index[i].c == c) {
            *color = index[i].color;
            return true;
        }
    }
    *color = 0;
    return false;
}

// We encounter some problems with SDL2_image on MSYS2 (Windows),
// so here is our own XPM parsing not to depend on SDL_image.
//
// We do not hardcode the binary image to keep some flexibility to replace the
// icon easily (just by replacing icon.xpm).
//
// Parameter is not "const char *" because XPM formats are generally stored in a
// (non-const) "char *"
SDL_Surface *
read_xpm(char *xpm[]) {
#ifndef NDEBUG
    // patch the XPM to change the icon color in debug mode
    xpm[2] = ".	c #CC00CC";
#endif

    char *endptr;
    // *** No error handling, assume the XPM source is valid ***
    // (it's in our source repo)
    // Assertions are only checked in debug
    int width = strtol(xpm[0], &endptr, 10);
    int height = strtol(endptr + 1, &endptr, 10);
    int colors = strtol(endptr + 1, &endptr, 10);
    int chars = strtol(endptr + 1, &endptr, 10);

    // sanity checks
    assert(0 <= width && width < 256);
    assert(0 <= height && height < 256);
    assert(0 <= colors && colors < 256);
    assert(chars == 1); // this implementation does not support more

    (void) chars;

    // init index
    struct index index[colors];
    for (int i = 0; i < colors; ++i) {
        const char *line = xpm[1+i];
        index[i].c = line[0];
        assert(line[1] == '\t');
        assert(line[2] == 'c');
        assert(line[3] == ' ');
        if (line[4] == '#') {
            index[i].color = 0xff000000 | strtol(&line[5], &endptr, 0x10);
            assert(*endptr == '\0');
        } else {
            assert(!strcmp("None", &line[4]));
            index[i].color = 0;
        }
    }

    // parse image
    uint32_t *pixels = SDL_malloc(4 * width * height);
    if (!pixels) {
        LOGE("Could not allocate icon memory");
        return NULL;
    }
    for (int y = 0; y < height; ++y) {
        const char *line = xpm[1 + colors + y];
        for (int x = 0; x < width; ++x) {
            char c = line[x];
            uint32_t color;
            bool color_found = find_color(index, colors, c, &color);
            assert(color_found);
            (void) color_found;
            pixels[y * width + x] = color;
        }
    }

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    uint32_t amask = 0x000000ff;
    uint32_t rmask = 0x0000ff00;
    uint32_t gmask = 0x00ff0000;
    uint32_t bmask = 0xff000000;
#else // little endian, like x86
    uint32_t amask = 0xff000000;
    uint32_t rmask = 0x00ff0000;
    uint32_t gmask = 0x0000ff00;
    uint32_t bmask = 0x000000ff;
#endif

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels,
                                                    width, height,
                                                    32, 4 * width,
                                                    rmask, gmask, bmask, amask);
    if (!surface) {
        LOGE("Could not create icon surface");
        return NULL;
    }
    // make the surface own the raw pixels
    surface->flags &= ~SDL_PREALLOC;
    return surface;
}
