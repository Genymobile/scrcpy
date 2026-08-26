#include "interop_vaapi.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

#include <drm_fourcc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>

#include "util/log.h"

/** Downcast interop to sc_interop_vaapi */
#define DOWNCAST(INTEROP) \
    container_of(INTEROP, struct sc_interop_vaapi, interop)

static_assert(SC_DRM_FORMAT_MOD_INVALID == DRM_FORMAT_MOD_INVALID,
              "SC_DRM_FORMAT_MOD_INVALID must match drm_fourcc.h");

static void
sc_interop_vaapi_destroy_images(struct sc_egl *egl, EGLImageKHR images[2]) {
    for (unsigned i = 0; i < 2; ++i) {
        if (images[i] != EGL_NO_IMAGE_KHR) {
            sc_egl_destroy_image(egl, images[i]);
            images[i] = EGL_NO_IMAGE_KHR;
        }
    }
}

static void
sc_interop_vaapi_unmap_frame(struct sc_interop_vaapi *vaapi) {
    sc_interop_vaapi_destroy_images(&vaapi->egl, vaapi->images);
    av_frame_free(&vaapi->drm_frame);
}

static void
sc_interop_vaapi_reset(struct sc_interop *interop) {
    struct sc_interop_vaapi *vaapi = DOWNCAST(interop);
    if (interop->texture) {
        sc_interop_vaapi_unmap_frame(vaapi);
        SDL_DestroyTexture(interop->texture);
        interop->texture = NULL;
    }
}

static void
sc_interop_vaapi_destroy(struct sc_interop *interop) {
    struct sc_interop_vaapi *vaapi = DOWNCAST(interop);
    if (interop->texture) {
        sc_interop_vaapi_unmap_frame(vaapi);
        SDL_DestroyTexture(interop->texture);
    }
    SDL_ResetHint("SDL_RENDER_OPENGL_NV12_RG_SHADER");
}

static bool
sc_interop_vaapi_check_nv12_layout(const AVDRMFrameDescriptor *desc) {
    return desc->nb_layers == 2
        && desc->layers[0].format == DRM_FORMAT_R8
        && desc->layers[0].nb_planes == 1
        && desc->layers[1].format == DRM_FORMAT_GR88
        && desc->layers[1].nb_planes == 1;
}

static EGLImageKHR
sc_interop_vaapi_import_plane(struct sc_egl *egl,
                              const AVDRMFrameDescriptor *desc, unsigned layer,
                              int width, int height) {
    const AVDRMLayerDescriptor *l = &desc->layers[layer];
    const AVDRMPlaneDescriptor *p = &l->planes[0];
    assert(p->object_index >= 0 && p->object_index < desc->nb_objects);
    const AVDRMObjectDescriptor *object = &desc->objects[p->object_index];

    struct sc_egl_dma_buf_plane plane = {
        .fd = object->fd,
        .offset = p->offset,
        .pitch = p->pitch,
        .modifier = object->format_modifier, // might be DRM_FORMAT_MOD_INVALID
    };

    return sc_egl_create_image_dma_buf(egl, l->format, width, height, &plane);
}

static bool
sc_interop_vaapi_create_texture(struct sc_interop_vaapi *vaapi,
                                struct sc_size size,
                                SDL_Colorspace colorspace) {
    LOGV("Creating new VA-API texture: size=%" PRIu16 "x%" PRIu16
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
        SDL_CreateTextureWithProperties(vaapi->renderer, props);
    SDL_DestroyProperties(props);
    if (!texture) {
        LOGD("Could not create texture: %s", SDL_GetError());
        return false;
    }

    static const char *const opengl_keys[2] = {
        SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER,
        SDL_PROP_TEXTURE_OPENGL_TEXTURE_UV_NUMBER,
    };

    static const char *const opengles2_keys[2] = {
        SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER,
        SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_UV_NUMBER,
    };

    const char *const *keys = vaapi->gl->is_opengles ? opengles2_keys
                                                     : opengl_keys;
    props = SDL_GetTextureProperties(texture);
    if (!props) {
        LOGE("Could not get texture properties: %s", SDL_GetError());
        SDL_DestroyTexture(texture);
        return false;
    }

    for (unsigned i = 0; i < 2; ++i) {
        int64_t texture_id = SDL_GetNumberProperty(props, keys[i], 0);
        if (!texture_id) {
            LOGE("Could not get texture id: %s", SDL_GetError());
            SDL_DestroyTexture(texture);
            return false;
        }

        assert(!(texture_id & ~0xFFFFFFFF)); // fits in uint32_t
        vaapi->texture_ids[i] = texture_id;
    }

    vaapi->interop.texture = texture;
    vaapi->interop.frame_size = size;
    LOGI("Texture (VA-API): %" PRIu16 "x%" PRIu16, size.width, size.height);
    return true;
}

static bool
sc_interop_vaapi_import(struct sc_interop *interop, const AVFrame *frame) {
    struct sc_interop_vaapi *vaapi = DOWNCAST(interop);

    assert(frame->format == AV_PIX_FMT_VAAPI);
    assert(frame->hw_frames_ctx);

    const AVHWFramesContext *frames =
        (const AVHWFramesContext *) frame->hw_frames_ctx->data;
    if (frames->sw_format != AV_PIX_FMT_NV12) {
        LOGE("VA-API: unsupported pixel format %s",
             av_get_pix_fmt_name(frames->sw_format));
        return false;
    }

    struct sc_size size = {frame->width, frame->height};
    assert(size.width && size.height);

    if (!interop->texture
            || interop->frame_size.width != size.width
            || interop->frame_size.height != size.height) {
        // Incompatible texture, recreate it
        sc_interop_vaapi_reset(interop);

        SDL_Colorspace colorspace =
            sc_interop_to_sdl_color_space(frame->colorspace,
                                          frame->color_range);
        bool ok = sc_interop_vaapi_create_texture(vaapi, size, colorspace);
        if (!ok) {
            return false;
        }
    }

    AVFrame *drm_frame = av_frame_alloc();
    if (!drm_frame) {
        LOG_OOM();
        return false;
    }
    drm_frame->format = AV_PIX_FMT_DRM_PRIME;
    int ret = av_hwframe_map(drm_frame, frame,
                             AV_HWFRAME_MAP_READ | AV_HWFRAME_MAP_DIRECT);
    if (ret < 0) {
        LOGE("VA-API: could not export the decoded frame as DMA-BUF: %s",
             av_err2str(ret));
        av_frame_free(&drm_frame);
        return false;
    }

    const AVDRMFrameDescriptor *desc =
        (const AVDRMFrameDescriptor *) drm_frame->data[0];
    assert(desc);
    if (!sc_interop_vaapi_check_nv12_layout(desc)) {
        uint32_t format = desc->nb_layers ? desc->layers[0].format : 0;
        LOGE("VA-API: unsupported DRM PRIME layout (layers=%d, format=%#08x)",
             desc->nb_layers, format);
        av_frame_free(&drm_frame);
        return false;
    }

    struct sc_egl *egl = &vaapi->egl;

    EGLImageKHR images[2];
    images[0] = sc_interop_vaapi_import_plane(egl, desc, 0,
                                              frame->width, frame->height);
    images[1] = sc_interop_vaapi_import_plane(egl, desc, 1,
                                              (frame->width + 1) / 2,
                                              (frame->height + 1) / 2);
    if (images[0] == EGL_NO_IMAGE_KHR || images[1] == EGL_NO_IMAGE_KHR) {
        LOGE("VA-API: could not import DMA-BUF as EGLImage (EGL error %#x)",
             vaapi->egl.GetError());
        sc_interop_vaapi_destroy_images(egl, images);
        av_frame_free(&drm_frame);
        return false;
    }

    // Invalidate SDL OpenGL cache before touching the textures behind its back
    if (!SDL_FlushRenderer(vaapi->renderer)) {
        LOGD("Could not flush renderer before DMA-BUF import: %s",
             SDL_GetError());
    }

    struct sc_opengl *gl = vaapi->gl;
    while (gl->GetError()); // Clear any errors left

    for (unsigned i = 0; i < 2; ++i) {
        gl->BindTexture(GL_TEXTURE_2D, vaapi->texture_ids[i]);
        egl->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, images[i]);
    }
    gl->BindTexture(GL_TEXTURE_2D, 0);

    GLenum gl_error = gl->GetError();
    if (gl_error) {
        LOGE("VA-API: could not attach EGLImages to the SDL texture "
             "(GL error %#x)", gl_error);
        sc_interop_vaapi_destroy_images(egl, images);
        av_frame_free(&drm_frame);
        return false;
    }

    // Release the previous frame only once the new one is attached
    sc_interop_vaapi_unmap_frame(vaapi);
    vaapi->images[0] = images[0];
    vaapi->images[1] = images[1];
    vaapi->drm_frame = drm_frame;

    return true;
}

// The reasons why VA-API is unavailable are logged at info level: whether it is
// an error depends on the caller (only if VA-API was explicitly requested)
static bool
sc_interop_vaapi_init_egl(struct sc_interop_vaapi *vaapi) {
    struct sc_egl *egl = &vaapi->egl;
    if (!sc_egl_init(egl)) {
        LOGI("VA-API: EGL not available");
        return false;
    }

    if (!egl->CreateImageKHR) {
        LOGI("VA-API: EGL_KHR_image_base not supported");
        return false;
    }

    if (!egl->has_dma_buf_import) {
        LOGI("VA-API: EGL_EXT_image_dma_buf_import not supported");
        return false;
    }

    if (!egl->EGLImageTargetTexture2DOES) {
        LOGI("VA-API: GL_OES_EGL_image not supported");
        return false;
    }

    return true;
}

struct sc_interop_vaapi *
sc_interop_vaapi_new(SDL_Renderer *renderer, struct sc_opengl *gl,
                     bool mipmaps) {
    if (!gl) {
        LOGI("VA-API not available: the renderer is not OpenGL or OpenGL ES");
        return NULL;
    }

    struct sc_interop_vaapi *vaapi = malloc(sizeof(*vaapi));
    if (!vaapi) {
        LOG_OOM();
        return NULL;
    }

    bool ok = sc_interop_vaapi_init_egl(vaapi);
    if (!ok) {
        free(vaapi);
        return NULL;
    }

    // SDL normally stores NV12 chroma in a GL_LUMINANCE_ALPHA texture and
    // samples it from .ra. An EGLImage imported from DRM_FORMAT_GR88 is a
    // two-channel texture, so it must be sampled from .rg instead.
    ok = SDL_SetHintWithPriority("SDL_RENDER_OPENGL_NV12_RG_SHADER", "1",
                                 SDL_HINT_OVERRIDE);
    if (!ok) {
        LOGE("Could not set the NV12 shader hint");
        free(vaapi);
        return NULL;
    }

    if (mipmaps) {
        LOGI("Trilinear filtering disabled for VA-API frames");
    }

    static const struct sc_interop_ops ops = {
        .import = sc_interop_vaapi_import,
        .reset = sc_interop_vaapi_reset,
        .destroy = sc_interop_vaapi_destroy,
    };

    vaapi->interop.name = "vaapi";
    vaapi->interop.hw_type = AV_HWDEVICE_TYPE_VAAPI;
    vaapi->interop.pix_fmt = AV_PIX_FMT_VAAPI;
    vaapi->interop.texture = NULL;
    vaapi->interop.ops = &ops;

    vaapi->renderer = renderer;
    vaapi->gl = gl;
    vaapi->images[0] = EGL_NO_IMAGE_KHR;
    vaapi->images[1] = EGL_NO_IMAGE_KHR;
    vaapi->drm_frame = NULL;

    return vaapi;
}
