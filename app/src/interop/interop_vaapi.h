#ifndef SC_INTEROP_VAAPI_H
#define SC_INTEROP_VAAPI_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include <libavutil/frame.h>

#include "egl.h"
#include "interop.h"
#include "opengl.h"

struct sc_interop_vaapi {
    struct sc_interop interop; // interop trait

    SDL_Renderer *renderer; // owned by the screen

    struct sc_opengl *gl;
    struct sc_egl egl;

    // Only valid if interop.texture != NULL
    uint32_t texture_ids[2]; // OpenGL textures of the Y and UV planes

    EGLImageKHR images[2];
    AVFrame *drm_frame;
};

struct sc_interop_vaapi *
sc_interop_vaapi_new(SDL_Renderer *renderer, struct sc_opengl *gl,
                     bool mipmaps);

#endif
