#ifndef SC_TEXTURE_H
#define SC_TEXTURE_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <SDL3/SDL.h>

#include "interop/interop.h"
#include "opengl.h"

struct sc_texture {
    SDL_Renderer *renderer; // owned by the caller

    struct sc_opengl gl;
    bool has_gl;
    bool mipmaps;

    struct sc_interop *interop;
};

bool
sc_texture_init(struct sc_texture *tex, SDL_Renderer *renderer, bool mipmaps,
                enum AVHWDeviceType hw_type);

void
sc_texture_destroy(struct sc_texture *tex);

SDL_Texture *
sc_texture_get(struct sc_texture *tex);

bool
sc_texture_update(struct sc_texture *tex, const AVFrame *frame);

void
sc_texture_reset(struct sc_texture *tex);

#endif
