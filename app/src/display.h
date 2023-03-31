#ifndef SC_DISPLAY_H
#define SC_DISPLAY_H

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL.h>

#include "coords.h"
#include "opengl.h"

struct sc_display {
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    struct sc_opengl gl;
    bool mipmaps;
};

bool
sc_display_init(struct sc_display *display, SDL_Window *window, bool mipmaps);

void
sc_display_destroy(struct sc_display *display);

bool
sc_display_set_texture_size(struct sc_display *display, struct sc_size size);

bool
sc_display_update_texture(struct sc_display *display, const AVFrame *frame);

bool
sc_display_render(struct sc_display *display, const SDL_Rect *geometry,
                  unsigned rotation);

#endif
