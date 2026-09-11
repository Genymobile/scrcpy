#ifndef SC_HWACCEL_H
#define SC_HWACCEL_H

#include "common.h"

#include <stdbool.h>

#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>

#ifdef SC_HAVE_VAAPI
# include "vaapi.h"
#elif defined(SC_HAVE_D3D11)
# ifndef COBJMACROS
#  define COBJMACROS
# endif
# include <libavutil/hwcontext_d3d11va.h>
#endif

struct sc_hwaccel {
#ifdef SC_HAVE_VAAPI
    struct sc_vaapi vaapi;
#elif defined(SC_HAVE_D3D11)
    ID3D11Device *device; // borrowed from the SDL renderer
    ID3D11DeviceContext *device_context;
#endif

    const char *unavailable_reason;
    bool available;
    bool rendering_failed;
    bool use_logged;
};

bool
sc_hwaccel_set_hints(void);

void
sc_hwaccel_init(struct sc_hwaccel *hwaccel, SDL_Renderer *renderer,
                bool enabled);

void
sc_hwaccel_destroy(struct sc_hwaccel *hwaccel);

void
sc_hwaccel_reset(struct sc_hwaccel *hwaccel);

bool
sc_hwaccel_configure_decoder(struct sc_hwaccel *hwaccel, AVCodecContext *ctx,
                             int buffered_frames);

bool
sc_hwaccel_is_frame(const AVFrame *frame);

SDL_PixelFormat
sc_hwaccel_get_texture_format(const AVFrame *frame);

bool
sc_hwaccel_needs_new_texture(SDL_Texture *texture, const AVFrame *frame);

SDL_Texture *
sc_hwaccel_create_texture(struct sc_hwaccel *hwaccel,
                          SDL_Renderer *renderer, const AVFrame *frame,
                          SDL_Colorspace colorspace);

bool
sc_hwaccel_update_texture(struct sc_hwaccel *hwaccel,
                          SDL_Renderer *renderer, SDL_Texture *texture,
                          const AVFrame *frame);

bool
sc_hwaccel_disable_rendering(struct sc_hwaccel *hwaccel);

bool
sc_hwaccel_prepare_software(struct sc_hwaccel *hwaccel);

#endif
