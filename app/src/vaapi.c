#include "vaapi.h"

#include "common.h"

#include <assert.h>
#include <string.h>

#include <drm_fourcc.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/pixfmt.h>

#include "util/log.h"

#define SC_GL_TEXTURE_2D 0x0DE1

static bool
sc_has_extension(const char *extensions, const char *extension) {
    if (!extensions || !extension || strchr(extension, ' ')) {
        return false;
    }

    size_t len = strlen(extension);
    const char *p = extensions;
    while ((p = strstr(p, extension))) {
        if ((p == extensions || p[-1] == ' ')
                && (p[len] == '\0' || p[len] == ' ')) {
            return true;
        }
        p += len;
    }
    return false;
}

static void
sc_vaapi_destroy_images(struct sc_vaapi *vaapi) {
    for (unsigned i = 0; i < 2; ++i) {
        if (vaapi->images[i] != EGL_NO_IMAGE_KHR) {
            vaapi->DestroyImageKHR(vaapi->display, vaapi->images[i]);
            vaapi->images[i] = EGL_NO_IMAGE_KHR;
        }
    }
}

bool
sc_vaapi_init(struct sc_vaapi *vaapi, SDL_Renderer *renderer,
              const char **unavailable_reason) {
    memset(vaapi, 0, sizeof(*vaapi));
    vaapi->images[0] = EGL_NO_IMAGE_KHR;
    vaapi->images[1] = EGL_NO_IMAGE_KHR;

    const char *renderer_name = SDL_GetRendererName(renderer);
    bool is_opengl = renderer_name && !strcmp(renderer_name, "opengl");
    bool is_opengles2 = renderer_name && !strcmp(renderer_name, "opengles2");
    if (!is_opengl && !is_opengles2) {
        *unavailable_reason =
            "the selected SDL renderer is not OpenGL/OpenGL ES";
        return false;
    }

    vaapi->display = (EGLDisplay) SDL_EGL_GetCurrentDisplay();
    if (vaapi->display == EGL_NO_DISPLAY) {
        *unavailable_reason =
            "the OpenGL renderer does not use EGL "
            "(try with SDL_VIDEO_FORCE_EGL=1)";
        return false;
    }

    PFNEGLQUERYSTRINGPROC QueryString = (PFNEGLQUERYSTRINGPROC)
        SDL_EGL_GetProcAddress("eglQueryString");
    if (!QueryString) {
        *unavailable_reason = "eglQueryString is unavailable";
        return false;
    }

    const char *egl_extensions = QueryString(vaapi->display, EGL_EXTENSIONS);
    if (!sc_has_extension(egl_extensions, "EGL_EXT_image_dma_buf_import")) {
        *unavailable_reason =
            "EGL_EXT_image_dma_buf_import is not supported";
        return false;
    }

    const unsigned char *(*GetString)(unsigned int) =
        (const unsigned char *(*)(unsigned int))
            SDL_GL_GetProcAddress("glGetString");
    if (!GetString) {
        *unavailable_reason = "glGetString is unavailable";
        return false;
    }
    const char *gl_extensions =
        (const char *) GetString(0x1F03 /* GL_EXTENSIONS */);
    if (!sc_has_extension(gl_extensions, "GL_OES_EGL_image")) {
        *unavailable_reason = "GL_OES_EGL_image is not supported";
        return false;
    }

    vaapi->CreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)
        SDL_EGL_GetProcAddress("eglCreateImageKHR");
    vaapi->DestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)
        SDL_EGL_GetProcAddress("eglDestroyImageKHR");
    vaapi->EglGetError = (PFNEGLGETERRORPROC)
        SDL_EGL_GetProcAddress("eglGetError");
    vaapi->BindTexture = (void (*)(unsigned int, unsigned int))
        SDL_GL_GetProcAddress("glBindTexture");
    vaapi->EGLImageTargetTexture2DOES =
        (void (*)(unsigned int, void *))
            SDL_GL_GetProcAddress("glEGLImageTargetTexture2DOES");
    vaapi->GetError = (unsigned int (*)(void))
        SDL_GL_GetProcAddress("glGetError");
    if (!vaapi->CreateImageKHR || !vaapi->DestroyImageKHR
            || !vaapi->EglGetError || !vaapi->BindTexture
            || !vaapi->EGLImageTargetTexture2DOES || !vaapi->GetError) {
        *unavailable_reason =
            "required EGLImage/OpenGL entry points are unavailable";
        return false;
    }

    vaapi->texture_prop = is_opengl
                        ? SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER
                        : SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER;
    vaapi->texture_uv_prop = is_opengl
                           ? SDL_PROP_TEXTURE_OPENGL_TEXTURE_UV_NUMBER
                           : SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_UV_NUMBER;
    vaapi->has_modifiers = sc_has_extension(
        egl_extensions, "EGL_EXT_image_dma_buf_import_modifiers");
    return true;
}

void
sc_vaapi_reset(struct sc_vaapi *vaapi) {
    sc_vaapi_destroy_images(vaapi);
    av_frame_free(&vaapi->drm_frame);
}

void
sc_vaapi_destroy(struct sc_vaapi *vaapi) {
    sc_vaapi_reset(vaapi);
}

bool
sc_vaapi_set_nv12_rg_shader(bool enabled) {
    // SDL normally stores NV12 chroma in a legacy GL_LUMINANCE_ALPHA texture
    // and samples it from .ra. An EGLImage imported from DRM_FORMAT_GR88 is a
    // two-channel texture, so it must be sampled from .rg instead. This hint is
    // intentionally set with override priority: both paths render incorrectly
    // if an environment hint forces the texture's other storage convention.
    return SDL_SetHintWithPriority("SDL_RENDER_OPENGL_NV12_RG_SHADER",
                                   enabled ? "1" : "0", SDL_HINT_OVERRIDE);
}

struct sc_vaapi_plane {
    const AVDRMObjectDescriptor *object;
    const AVDRMPlaneDescriptor *plane;
};

static bool
sc_vaapi_find_nv12_planes(const AVDRMFrameDescriptor *desc,
                          struct sc_vaapi_plane planes[2]) {
    if (desc->nb_layers == 1) {
        const AVDRMLayerDescriptor *layer = &desc->layers[0];
        if (layer->format != DRM_FORMAT_NV12 || layer->nb_planes != 2) {
            return false;
        }
        for (unsigned i = 0; i < 2; ++i) {
            int object_index = layer->planes[i].object_index;
            if (object_index < 0 || object_index >= desc->nb_objects) {
                return false;
            }
            planes[i].object = &desc->objects[object_index];
            planes[i].plane = &layer->planes[i];
        }
        return true;
    }

    if (desc->nb_layers == 2
            && desc->layers[0].format == DRM_FORMAT_R8
            && desc->layers[1].format == DRM_FORMAT_GR88
            && desc->layers[0].nb_planes == 1
            && desc->layers[1].nb_planes == 1) {
        for (unsigned i = 0; i < 2; ++i) {
            const AVDRMPlaneDescriptor *plane = &desc->layers[i].planes[0];
            if (plane->object_index < 0
                    || plane->object_index >= desc->nb_objects) {
                return false;
            }
            planes[i].object = &desc->objects[plane->object_index];
            planes[i].plane = plane;
        }
        return true;
    }

    return false;
}

static EGLImageKHR
sc_vaapi_import_plane(struct sc_vaapi *vaapi,
                      const struct sc_vaapi_plane *plane, uint32_t format,
                      int width, int height) {
    EGLint attrs[20];
    unsigned n = 0;
    attrs[n++] = EGL_WIDTH;
    attrs[n++] = width;
    attrs[n++] = EGL_HEIGHT;
    attrs[n++] = height;
    attrs[n++] = EGL_LINUX_DRM_FOURCC_EXT;
    attrs[n++] = (EGLint) format;
    attrs[n++] = EGL_DMA_BUF_PLANE0_FD_EXT;
    attrs[n++] = plane->object->fd;
    attrs[n++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attrs[n++] = (EGLint) plane->plane->offset;
    attrs[n++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attrs[n++] = (EGLint) plane->plane->pitch;

    uint64_t modifier = plane->object->format_modifier;
    if (vaapi->has_modifiers && modifier != DRM_FORMAT_MOD_INVALID) {
        attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attrs[n++] = (EGLint) (modifier & UINT32_MAX);
        attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attrs[n++] = (EGLint) (modifier >> 32);
    }
    attrs[n++] = EGL_NONE;
    assert(n <= ARRAY_LEN(attrs));

    return vaapi->CreateImageKHR(vaapi->display, EGL_NO_CONTEXT,
                                 EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
}

bool
sc_vaapi_set_frame(struct sc_vaapi *vaapi, SDL_Renderer *renderer,
                   SDL_Texture *texture, const AVFrame *frame) {
    assert(frame->format == AV_PIX_FMT_VAAPI);

    AVFrame *drm_frame = av_frame_alloc();
    if (!drm_frame) {
        LOG_OOM();
        return false;
    }
    drm_frame->format = AV_PIX_FMT_DRM_PRIME;
    int ret = av_hwframe_map(drm_frame, frame,
                             AV_HWFRAME_MAP_READ | AV_HWFRAME_MAP_DIRECT);
    if (ret < 0) {
        char err[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err, sizeof(err));
        LOGW("VA-API zero-copy unavailable: could not export the decoded "
             "frame as DMA-BUF: %s", err);
        av_frame_free(&drm_frame);
        return false;
    }

    const AVDRMFrameDescriptor *desc =
        (const AVDRMFrameDescriptor *) drm_frame->data[0];
    struct sc_vaapi_plane planes[2];
    if (!desc || !sc_vaapi_find_nv12_planes(desc, planes)) {
        uint32_t format = desc && desc->nb_layers ? desc->layers[0].format : 0;
        LOGW("VA-API zero-copy unavailable: unsupported DRM PRIME layout "
             "(layers=%d, format=%#08x)", desc ? desc->nb_layers : 0,
             format);
        av_frame_free(&drm_frame);
        return false;
    }

    EGLImageKHR images[2];
    images[0] = sc_vaapi_import_plane(vaapi, &planes[0], DRM_FORMAT_R8,
                                      frame->width, frame->height);
    images[1] = sc_vaapi_import_plane(vaapi, &planes[1], DRM_FORMAT_GR88,
                                      (frame->width + 1) / 2,
                                      (frame->height + 1) / 2);
    if (images[0] == EGL_NO_IMAGE_KHR || images[1] == EGL_NO_IMAGE_KHR) {
        LOGW("VA-API zero-copy unavailable: could not import DMA-BUF as "
             "EGLImage (EGL error %#x)", vaapi->EglGetError());
        for (unsigned i = 0; i < 2; ++i) {
            if (images[i] != EGL_NO_IMAGE_KHR) {
                vaapi->DestroyImageKHR(vaapi->display, images[i]);
            }
        }
        av_frame_free(&drm_frame);
        return false;
    }

    SDL_PropertiesID props = SDL_GetTextureProperties(texture);
    unsigned int texture_y =
        SDL_GetNumberProperty(props, vaapi->texture_prop, 0);
    unsigned int texture_uv =
        SDL_GetNumberProperty(props, vaapi->texture_uv_prop, 0);
    if (!texture_y || !texture_uv) {
        LOGW("VA-API zero-copy unavailable: SDL did not expose its NV12 "
             "OpenGL textures");
        vaapi->DestroyImageKHR(vaapi->display, images[0]);
        vaapi->DestroyImageKHR(vaapi->display, images[1]);
        av_frame_free(&drm_frame);
        return false;
    }

    if (!SDL_FlushRenderer(renderer)) {
        LOGD("Could not flush renderer before DMA-BUF import: %s",
             SDL_GetError());
    }
    while (vaapi->GetError()) {
        // Clear errors left by the renderer before checking our GL calls.
    }
    vaapi->BindTexture(SC_GL_TEXTURE_2D, texture_y);
    vaapi->EGLImageTargetTexture2DOES(SC_GL_TEXTURE_2D, images[0]);
    vaapi->BindTexture(SC_GL_TEXTURE_2D, texture_uv);
    vaapi->EGLImageTargetTexture2DOES(SC_GL_TEXTURE_2D, images[1]);
    vaapi->BindTexture(SC_GL_TEXTURE_2D, 0);
    unsigned int gl_error = vaapi->GetError();
    if (gl_error) {
        LOGW("VA-API zero-copy unavailable: could not attach EGLImages to "
             "the SDL texture (GL error %#x)", gl_error);
        vaapi->DestroyImageKHR(vaapi->display, images[0]);
        vaapi->DestroyImageKHR(vaapi->display, images[1]);
        av_frame_free(&drm_frame);
        return false;
    }

    sc_vaapi_destroy_images(vaapi);
    av_frame_free(&vaapi->drm_frame);
    vaapi->images[0] = images[0];
    vaapi->images[1] = images[1];
    vaapi->drm_frame = drm_frame;
    return true;
}
