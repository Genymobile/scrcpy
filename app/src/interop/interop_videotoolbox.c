#include "interop_videotoolbox.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>

#include "util/log.h"

/** Downcast interop to sc_interop_videotoolbox */
#define DOWNCAST(INTEROP) \
    container_of(INTEROP, struct sc_interop_videotoolbox, interop)

static void
sc_interop_videotoolbox_reset(struct sc_interop *interop) {
    if (interop->texture) {
        SDL_DestroyTexture(interop->texture);
        interop->texture = NULL;
    }
}

static void
sc_interop_videotoolbox_destroy(struct sc_interop *interop) {
    if (interop->texture) {
        SDL_DestroyTexture(interop->texture);
    }
}

static SDL_Texture *
sc_interop_videotoolbox_create_texture(struct sc_interop_videotoolbox *vt,
                                       struct sc_size size,
                                       SDL_Colorspace colorspace,
                                       void *pixel_buffer) {
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        LOG_OOM();
        return NULL;
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
    // The Metal texture is created from the IOSurface of the pixel buffer
    const char *key = SDL_PROP_TEXTURE_CREATE_METAL_PIXELBUFFER_POINTER;
    ok &= SDL_SetPointerProperty(props, key, pixel_buffer);
    if (!ok) {
        LOGE("Could not set texture properties");
        SDL_DestroyProperties(props);
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureWithProperties(vt->renderer, props);
    SDL_DestroyProperties(props);
    if (!texture) {
        LOGD("Could not create texture: %s", SDL_GetError());
        return NULL;
    }

    return texture;
}

static bool
sc_interop_videotoolbox_import(struct sc_interop *interop,
                               const AVFrame *frame) {
    struct sc_interop_videotoolbox *vt = DOWNCAST(interop);

    assert(frame->format == AV_PIX_FMT_VIDEOTOOLBOX);
    assert(frame->hw_frames_ctx);

    const AVHWFramesContext *frames =
        (const AVHWFramesContext *) frame->hw_frames_ctx->data;
    if (frames->sw_format != AV_PIX_FMT_NV12) {
        LOGE("VideoToolbox: unsupported pixel format %s",
             av_get_pix_fmt_name(frames->sw_format));
        return false;
    }

    void *pixel_buffer = frame->data[3];
    assert(pixel_buffer);

    struct sc_size size = {frame->width, frame->height};
    assert(size.width && size.height);

    bool size_changed = !interop->texture
                     || interop->frame_size.width != size.width
                     || interop->frame_size.height != size.height;

    SDL_Colorspace colorspace =
        sc_interop_to_sdl_color_space(frame->colorspace, frame->color_range);
    SDL_Texture *texture =
        sc_interop_videotoolbox_create_texture(vt, size, colorspace,
                                               pixel_buffer);
    if (!texture) {
        return false;
    }

    // Release the previous frame only once the new one is wrapped
    sc_interop_videotoolbox_reset(interop);
    interop->texture = texture;
    interop->frame_size = size;

    if (size_changed) {
        LOGI("Texture (VideoToolbox): %" PRIu16 "x%" PRIu16, size.width,
             size.height);
    }

    return true;
}

// The reasons why VideoToolbox is unavailable are logged at info level: whether
// it is an error depends on the caller (only if VideoToolbox was explicitly
// requested)
struct sc_interop_videotoolbox *
sc_interop_videotoolbox_new(SDL_Renderer *renderer) {
    const char *renderer_name = SDL_GetRendererName(renderer);
    if (!renderer_name || strcmp(renderer_name, "metal")) {
        LOGI("VideoToolbox not available: the renderer is not Metal");
        return NULL;
    }

    struct sc_interop_videotoolbox *vt = malloc(sizeof(*vt));
    if (!vt) {
        LOG_OOM();
        return NULL;
    }

    static const struct sc_interop_ops ops = {
        .import = sc_interop_videotoolbox_import,
        .reset = sc_interop_videotoolbox_reset,
        .destroy = sc_interop_videotoolbox_destroy,
    };

    vt->interop.name = "videotoolbox";
    vt->interop.hw_type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    vt->interop.pix_fmt = AV_PIX_FMT_VIDEOTOOLBOX;
    vt->interop.texture = NULL;
    vt->interop.ops = &ops;

    vt->renderer = renderer;

    return vt;
}
