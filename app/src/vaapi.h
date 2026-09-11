#ifndef SC_VAAPI_H
#define SC_VAAPI_H

#include <stdbool.h>
#include <stdint.h>

// Use the EGL definitions bundled with SDL, so that no external EGL
// development package is required
#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS
#include <SDL3/SDL_egl.h>
#include <SDL3/SDL.h>
#include <libavutil/frame.h>

struct sc_vaapi {
    EGLDisplay display;
    PFNEGLCREATEIMAGEKHRPROC CreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC DestroyImageKHR;
    PFNEGLGETERRORPROC EglGetError;
    void (*BindTexture)(unsigned int target, unsigned int texture);
    void (*EGLImageTargetTexture2DOES)(unsigned int target, void *image);
    unsigned int (*GetError)(void);

    EGLImageKHR images[2];
    AVFrame *drm_frame;

    const char *texture_prop;
    const char *texture_uv_prop;
    bool has_modifiers;
};

bool
sc_vaapi_init(struct sc_vaapi *vaapi, SDL_Renderer *renderer,
              const char **unavailable_reason);

void
sc_vaapi_destroy(struct sc_vaapi *vaapi);

void
sc_vaapi_reset(struct sc_vaapi *vaapi);

bool
sc_vaapi_set_frame(struct sc_vaapi *vaapi, SDL_Renderer *renderer,
                   SDL_Texture *texture, const AVFrame *frame);

bool
sc_vaapi_set_nv12_rg_shader(bool enabled);

#endif
