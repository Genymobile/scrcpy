#ifndef SC_TEXTURE_H
#define SC_TEXTURE_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/frame.h>
#include <SDL3/SDL.h>

#include "coords.h"
#include "opengl.h"

struct sc_texture {
    SDL_Renderer *renderer; // owned by the caller
    SDL_Texture *texture;
    // Only valid if texture != NULL
    struct sc_size texture_size;

    struct sc_opengl gl;

    bool mipmaps;
    uint32_t texture_ids[3]; // only set if mipmaps is enabled
};

bool
sc_texture_init(struct sc_texture *tex, SDL_Renderer *renderer, bool mipmaps);

void
sc_texture_destroy(struct sc_texture *tex);

bool
sc_texture_update(struct sc_texture *tex, const AVFrame *frame);

void
sc_texture_reset(struct sc_texture *tex);

#endif
