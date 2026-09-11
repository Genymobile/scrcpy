#ifndef SC_DISPLAY_H
#define SC_DISPLAY_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavutil/frame.h>
#include <SDL3/SDL.h>

#include "coords.h"
#include "opengl.h"
#ifdef HAVE_HWACCEL
# include "hwaccel.h"
#endif

enum sc_texture_type {
    SC_TEXTURE_TYPE_FRAME,
    SC_TEXTURE_TYPE_ICON,
};

struct sc_texture {
    SDL_Renderer *renderer; // owned by the caller
    SDL_Texture *texture;
    // Only valid if texture != NULL
    struct sc_size texture_size;
    enum sc_texture_type texture_type;
    SDL_PixelFormat texture_format;

    struct sc_opengl gl;

    bool mipmaps;
    bool texture_hardware;
    uint32_t texture_id; // only set if mipmaps is enabled
#ifdef HAVE_HWACCEL
    struct sc_hwaccel hwaccel;
#endif
};

bool
sc_texture_init(struct sc_texture *tex, SDL_Renderer *renderer, bool mipmaps,
                bool hardware_decoding);

bool
sc_texture_supports_hardware_decoding(const struct sc_texture *tex);

const char *
sc_texture_get_hardware_decoding_unavailable_reason(
    const struct sc_texture *tex);

#ifdef HAVE_HWACCEL
struct sc_hwaccel *
sc_texture_get_hwaccel(struct sc_texture *tex);
#endif

void
sc_texture_destroy(struct sc_texture *tex);

bool
sc_texture_set_from_frame(struct sc_texture *tex, const AVFrame *frame);

bool
sc_texture_set_from_surface(struct sc_texture *tex, SDL_Surface *surface);

void
sc_texture_reset(struct sc_texture *tex);

#endif
