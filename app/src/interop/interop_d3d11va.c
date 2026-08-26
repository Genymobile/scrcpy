#include "interop_d3d11va.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>

#include "util/log.h"

/** Downcast interop to sc_interop_d3d11va */
#define DOWNCAST(INTEROP) \
    container_of(INTEROP, struct sc_interop_d3d11va, interop)

static void
sc_interop_d3d11va_reset(struct sc_interop *interop) {
    if (interop->texture) {
        SDL_DestroyTexture(interop->texture);
        interop->texture = NULL;
    }
}

static void
sc_interop_d3d11va_destroy(struct sc_interop *interop) {
    struct sc_interop_d3d11va *d3d11va = DOWNCAST(interop);
    if (interop->texture) {
        SDL_DestroyTexture(interop->texture);
    }
    ID3D11DeviceContext_Release(d3d11va->device_ctx);
}

static bool
sc_interop_d3d11va_create_texture(struct sc_interop_d3d11va *d3d11va,
                                  struct sc_size size,
                                  SDL_Colorspace colorspace) {
    LOGV("Creating new D3D11VA texture: size=%" PRIu16 "x%" PRIu16
         " color_space=%d ", size.width, size.height, colorspace);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        LOG_OOM();
        return false;
    }

    bool ok =
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              SDL_PIXELFORMAT_NV12);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                                SDL_TEXTUREACCESS_STATIC);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,
                                size.width);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,
                                size.height);
    ok &= SDL_SetNumberProperty(props,
                                SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,
                                colorspace);
    if (!ok) {
        LOGE("Could not set texture properties");
        SDL_DestroyProperties(props);
        return false;
    }

    SDL_Texture *texture =
        SDL_CreateTextureWithProperties(d3d11va->renderer, props);
    SDL_DestroyProperties(props);
    if (!texture) {
        LOGD("Could not create texture: %s", SDL_GetError());
        return false;
    }

    d3d11va->interop.texture = texture;
    d3d11va->texture_size = size;
    LOGI("Texture (D3D11VA): %" PRIu16 "x%" PRIu16, size.width, size.height);
    return true;
}

static bool
sc_interop_d3d11va_import(struct sc_interop *interop, const AVFrame *frame) {
    struct sc_interop_d3d11va *d3d11va = DOWNCAST(interop);

    assert(frame->format == AV_PIX_FMT_D3D11);
    assert(frame->hw_frames_ctx);

    const AVHWFramesContext *frames =
        (const AVHWFramesContext *) frame->hw_frames_ctx->data;
    if (frames->sw_format != AV_PIX_FMT_NV12) {
        LOGE("D3D11VA: unsupported pixel format %s",
             av_get_pix_fmt_name(frames->sw_format));
        return false;
    }

    struct sc_size frame_size = {frame->width, frame->height};
    assert(frame_size.width && frame_size.height);

    struct sc_size surface_size = {frames->width, frames->height};

    if (!interop->texture
            || d3d11va->texture_size.width != surface_size.width
            || d3d11va->texture_size.height != surface_size.height) {
        // Incompatible texture, recreate it
        sc_interop_d3d11va_reset(interop);

        SDL_Colorspace colorspace =
            sc_interop_to_sdl_color_space(frame->colorspace,
                                          frame->color_range);
        bool ok = sc_interop_d3d11va_create_texture(d3d11va, surface_size,
                                                    colorspace);
        if (!ok) {
            return false;
        }
    }

    // The properties are owned by the texture
    SDL_PropertiesID props = SDL_GetTextureProperties(interop->texture);
    if (!props) {
        LOGE("Could not get texture properties: %s", SDL_GetError());
        return false;
    }

    ID3D11Resource *dst =
        SDL_GetPointerProperty(props, SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER,
                               NULL);
    if (!dst) {
        LOGE("D3D11VA: SDL did not expose its texture");
        return false;
    }

    // The decoded surface is a slice of a texture array
    ID3D11Resource *src = (ID3D11Resource *) frame->data[0];
    UINT slice = (UINT) (uintptr_t) frame->data[1];
    assert(src);

    if (!SDL_FlushRenderer(d3d11va->renderer)) {
        LOGD("Could not flush renderer before D3D11VA surface copy: %s",
             SDL_GetError());
    }

    ID3D11DeviceContext_CopySubresourceRegion(d3d11va->device_ctx, dst,
                                              0, 0, 0, 0, src, slice, NULL);

    interop->frame_size = frame_size;
    return true;
}

// The reasons why D3D11VA is unavailable are logged at info level: whether it
// is an error depends on the caller (only if D3D11VA was explicitly requested)
struct sc_interop_d3d11va *
sc_interop_d3d11va_new(SDL_Renderer *renderer) {
    const char *renderer_name = SDL_GetRendererName(renderer);
    if (!renderer_name || strcmp(renderer_name, "direct3d11")) {
        LOGI("D3D11VA not available: the renderer is not Direct3D 11");
        return NULL;
    }

    // The properties are owned by the renderer
    SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
    ID3D11Device *device =
        SDL_GetPointerProperty(props, SDL_PROP_RENDERER_D3D11_DEVICE_POINTER,
                               NULL);
    if (!device) {
        LOGI("D3D11VA not available: SDL did not expose its Direct3D 11 "
             "device");
        return NULL;
    }

    ID3D11DeviceContext *device_ctx = NULL;
    ID3D11Device_GetImmediateContext(device, &device_ctx);
    if (!device_ctx) {
        LOGI("D3D11VA not available: could not get the Direct3D 11 immediate "
             "context");
        return NULL;
    }

    struct sc_interop_d3d11va *d3d11va = malloc(sizeof(*d3d11va));
    if (!d3d11va) {
        LOG_OOM();
        ID3D11DeviceContext_Release(device_ctx);
        return NULL;
    }

    static const struct sc_interop_ops ops = {
        .import = sc_interop_d3d11va_import,
        .reset = sc_interop_d3d11va_reset,
        .destroy = sc_interop_d3d11va_destroy,
    };

    d3d11va->interop.name = "d3d11va";
    d3d11va->interop.hw_type = AV_HWDEVICE_TYPE_D3D11VA;
    d3d11va->interop.pix_fmt = AV_PIX_FMT_D3D11;
    d3d11va->interop.texture = NULL;
    d3d11va->interop.ops = &ops;

    d3d11va->renderer = renderer;
    d3d11va->device_ctx = device_ctx;

    return d3d11va;
}
