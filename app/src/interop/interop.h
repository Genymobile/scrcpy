#ifndef SC_INTEROP_H
#define SC_INTEROP_H

#include "common.h"

#include <stdbool.h>

#include <SDL3/SDL.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>

#include "coords.h"
#include "opengl.h"
#include "options.h"

struct sc_interop {
    const char *name;

    enum AVHWDeviceType hw_type;
    enum AVPixelFormat pix_fmt;

    SDL_Texture *texture;
    struct sc_size frame_size; // only valid when texture != NULL

    const struct sc_interop_ops *ops;
};

struct sc_interop_ops {
    bool
    (*import)(struct sc_interop *interop, const AVFrame *frame);

    // Remove the current frame
    void
    (*reset)(struct sc_interop *interop);

    void
    (*destroy)(struct sc_interop *interop);
};

struct sc_interop *
sc_interop_new(enum sc_hwdec_mode hwdec_mode, SDL_Renderer *renderer,
               struct sc_opengl *gl, bool mipmaps);

void
sc_interop_delete(struct sc_interop *interop);

// Helpers shared by the interop implementations

SDL_Colorspace
sc_interop_to_sdl_color_space(enum AVColorSpace color_space,
                              enum AVColorRange color_range);

#endif
