#include "texture.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#ifdef HAVE_HWACCEL
# include <libavutil/error.h>
#endif
#include <libavutil/pixfmt.h>
#ifdef HAVE_HWACCEL
# include <libavutil/hwcontext.h>
#endif

#include "util/log.h"

bool
sc_texture_init(struct sc_texture *tex, SDL_Renderer *renderer, bool mipmaps,
                bool hardware_decoding) {
    const char *renderer_name = SDL_GetRendererName(renderer);
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    tex->mipmaps = false;

    // starts with "opengl"
    bool use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (use_opengl) {
        struct sc_opengl *gl = &tex->gl;
        sc_opengl_init(gl);

        LOGI("OpenGL version: %s", gl->version);

        if (mipmaps) {
            bool supports_mipmaps =
                sc_opengl_version_at_least(gl, 3, 0, /* OpenGL 3.0+ */
                                               2, 0  /* OpenGL ES 2.0+ */);
            if (supports_mipmaps) {
                LOGI("Trilinear filtering enabled");
                tex->mipmaps = true;
            } else {
                LOGW("Trilinear filtering disabled "
                     "(OpenGL 3.0+ or ES 2.0+ required)");
            }
        } else {
            LOGI("Trilinear filtering disabled");
        }
    } else if (mipmaps) {
        LOGD("Trilinear filtering disabled (not an OpenGL renderer)");
    }

    tex->renderer = renderer;
    tex->texture = NULL;
    tex->texture_hardware = false;
#ifdef HAVE_HWACCEL
    sc_hwaccel_init(&tex->hwaccel, renderer, hardware_decoding);
#else
    (void) hardware_decoding;
#endif
    return true;
}

bool
sc_texture_supports_hardware_decoding(const struct sc_texture *tex) {
#ifdef HAVE_HWACCEL
    return tex->hwaccel.available;
#else
    (void) tex;
    return false;
#endif
}

const char *
sc_texture_get_hardware_decoding_unavailable_reason(
        const struct sc_texture *tex) {
#ifdef HAVE_HWACCEL
    return tex->hwaccel.unavailable_reason;
#else
    (void) tex;
    return "hardware decoding support was not compiled in";
#endif
}

#ifdef HAVE_HWACCEL
struct sc_hwaccel *
sc_texture_get_hwaccel(struct sc_texture *tex) {
    assert(tex->hwaccel.available);
    return &tex->hwaccel;
}
#endif

static void
sc_texture_destroy_texture(struct sc_texture *tex) {
    if (tex->texture) {
        SDL_DestroyTexture(tex->texture);
        tex->texture = NULL;
    }
#ifdef HAVE_HWACCEL
    sc_hwaccel_reset(&tex->hwaccel);
#endif
}

void
sc_texture_destroy(struct sc_texture *tex) {
    sc_texture_destroy_texture(tex);
#ifdef HAVE_HWACCEL
    sc_hwaccel_destroy(&tex->hwaccel);
#endif
}

static enum SDL_Colorspace
sc_texture_to_sdl_color_space(enum AVColorSpace color_space,
                              enum AVColorRange color_range) {
    bool full_range = color_range == AVCOL_RANGE_JPEG;

    switch (color_space) {
        case AVCOL_SPC_BT709:
        case AVCOL_SPC_RGB:
        case AVCOL_SPC_UNSPECIFIED:
        case AVCOL_SPC_YCGCO:
            return full_range ? SDL_COLORSPACE_BT709_FULL
                              : SDL_COLORSPACE_BT709_LIMITED;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M:
            return full_range ? SDL_COLORSPACE_BT601_FULL
                              : SDL_COLORSPACE_BT601_LIMITED;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            return full_range ? SDL_COLORSPACE_BT2020_FULL
                              : SDL_COLORSPACE_BT2020_LIMITED;
        default:
            return SDL_COLORSPACE_JPEG;
    }
}

static SDL_Texture *
sc_texture_create_frame_texture(struct sc_texture *tex,
                                struct sc_size size,
                                SDL_PixelFormat format,
                                enum AVColorSpace color_space,
                                enum AVColorRange color_range) {
    LOGV("Creating new texture: size=%" PRIu16 "x%" PRIu16 " color_space=%d "
         "color_range=%d", size.width, size.height, color_space, color_range);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return NULL;
    }

    enum SDL_Colorspace sdl_color_space =
        sc_texture_to_sdl_color_space(color_space, color_range);

    bool ok =
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              format);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                                SDL_TEXTUREACCESS_STREAMING);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,
                                size.width);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,
                                size.height);
    ok &= SDL_SetNumberProperty(props,
                                SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,
                                sdl_color_space);

    if (!ok) {
        LOGE("Could not set texture properties");
        SDL_DestroyProperties(props);
        return NULL;
    }

    SDL_Renderer *renderer = tex->renderer;
    SDL_Texture *texture = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    if (!texture) {
        LOGD("Could not create texture: %s", SDL_GetError());
        return NULL;
    }

    if (tex->mipmaps && format == SDL_PIXELFORMAT_YV12) {
        struct sc_opengl *gl = &tex->gl;

        // The properties are owned by the texture
        SDL_PropertiesID props = SDL_GetTextureProperties(texture);
        if (!props) {
            LOGE("Could not get texture properties: %s", SDL_GetError());
            return NULL;
        }

        const char *renderer_name = SDL_GetRendererName(tex->renderer);
        const char *key = !renderer_name || !strcmp(renderer_name, "opengl")
                        ? SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER
                        : SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER;

        int64_t texture_id = SDL_GetNumberProperty(props, key, 0);
        if (!texture_id) {
            LOGE("Could not get texture id: %s", SDL_GetError());
            SDL_DestroyTexture(texture);
            return NULL;
        }

        assert(!(texture_id & ~0xFFFFFFFF)); // fits in uint32_t
        tex->texture_id = texture_id;
        gl->BindTexture(GL_TEXTURE_2D, tex->texture_id);

        // Enable trilinear filtering for downscaling
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          GL_LINEAR_MIPMAP_LINEAR);
        gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.f);

        gl->BindTexture(GL_TEXTURE_2D, 0);
    }

    return texture;
}

static bool
sc_texture_prepare_frame_texture(struct sc_texture *tex, const AVFrame *frame,
                                 SDL_PixelFormat format) {
    struct sc_size size = {frame->width, frame->height};
    assert(size.width && size.height);

    if (!tex->texture
            || tex->texture_type != SC_TEXTURE_TYPE_FRAME
            || tex->texture_hardware
            || tex->texture_format != format
            || tex->texture_size.width != size.width
            || tex->texture_size.height != size.height) {
        // Incompatible texture, recreate it
        enum AVColorSpace color_space = frame->colorspace;
        enum AVColorRange color_range = frame->color_range;

        sc_texture_destroy_texture(tex);

        tex->texture = sc_texture_create_frame_texture(tex, size, format,
                                                       color_space,
                                                       color_range);
        if (!tex->texture) {
            return false;
        }

        tex->texture_size = size;
        tex->texture_type = SC_TEXTURE_TYPE_FRAME;
        tex->texture_format = format;
        tex->texture_hardware = false;

        LOGI("Texture: %" PRIu16 "x%" PRIu16, size.width, size.height);
    }

    assert(tex->texture);
    assert(tex->texture_type == SC_TEXTURE_TYPE_FRAME);
    assert(tex->texture_format == format);

    return true;
}

static bool
sc_texture_set_from_sw_frame(struct sc_texture *tex, const AVFrame *frame) {
    SDL_PixelFormat format;
    switch (frame->format) {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            format = SDL_PIXELFORMAT_YV12;
            break;
        case AV_PIX_FMT_NV12:
            format = SDL_PIXELFORMAT_NV12;
            break;
        case AV_PIX_FMT_P010:
            format = SDL_PIXELFORMAT_P010;
            break;
        default:
            LOGD("Unsupported software pixel format: %d", frame->format);
            return false;
    }

    if (!sc_texture_prepare_frame_texture(tex, frame, format)) {
        return false;
    }

    bool ok;
    if (format == SDL_PIXELFORMAT_NV12
            || format == SDL_PIXELFORMAT_P010) {
        ok = SDL_UpdateNVTexture(tex->texture, NULL,
                                 frame->data[0], frame->linesize[0],
                                 frame->data[1], frame->linesize[1]);
    } else {
        ok = SDL_UpdateYUVTexture(tex->texture, NULL,
                                  frame->data[0], frame->linesize[0],
                                  frame->data[1], frame->linesize[1],
                                  frame->data[2], frame->linesize[2]);
    }
    if (!ok) {
        LOGD("Could not update texture: %s", SDL_GetError());
        return false;
    }

    if (tex->mipmaps && format == SDL_PIXELFORMAT_YV12) {
        assert(tex->texture_id);
        struct sc_opengl *gl = &tex->gl;

        gl->BindTexture(GL_TEXTURE_2D, tex->texture_id);
        gl->GenerateMipmap(GL_TEXTURE_2D);
        gl->BindTexture(GL_TEXTURE_2D, 0);
    }

    return true;
}

#ifdef HAVE_HWACCEL
static bool
sc_texture_set_from_hw_frame(struct sc_texture *tex, const AVFrame *frame) {
    struct sc_size size = {frame->width, frame->height};
    assert(size.width && size.height);

    SDL_PixelFormat format = sc_hwaccel_get_texture_format(frame);
    if (format == SDL_PIXELFORMAT_UNKNOWN) {
        return false;
    }

    bool metadata_changed = !tex->texture
                         || tex->texture_type != SC_TEXTURE_TYPE_FRAME
                         || !tex->texture_hardware
                         || tex->texture_format != format
                         || tex->texture_size.width != size.width
                         || tex->texture_size.height != size.height;
    if (metadata_changed
            || sc_hwaccel_needs_new_texture(tex->texture, frame)) {
        sc_texture_destroy_texture(tex);

        SDL_Colorspace colorspace = sc_texture_to_sdl_color_space(
            frame->colorspace, frame->color_range);
        tex->texture = sc_hwaccel_create_texture(
            &tex->hwaccel, tex->renderer, frame, colorspace);
        if (!tex->texture) {
            return false;
        }

        tex->texture_size = size;
        tex->texture_type = SC_TEXTURE_TYPE_FRAME;
        tex->texture_format = format;
        tex->texture_hardware = true;

        if (metadata_changed) {
            LOGI("Texture: %" PRIu16 "x%" PRIu16,
                 size.width, size.height);
        }
    }

    return sc_hwaccel_update_texture(&tex->hwaccel, tex->renderer,
                                     tex->texture, frame);
}
#endif

bool
sc_texture_set_from_frame(struct sc_texture *tex, const AVFrame *frame) {
#ifdef HAVE_HWACCEL
    if (sc_hwaccel_is_frame(frame)) {
        if (tex->hwaccel.available && !tex->hwaccel.rendering_failed) {
            if (sc_texture_set_from_hw_frame(tex, frame)) {
                return true;
            }
            if (!sc_hwaccel_disable_rendering(&tex->hwaccel)) {
                return false;
            }
        }

        // Keep playback working if the platform's native surface cannot be
        // imported by this renderer/driver combination.
        AVFrame *sw_frame = av_frame_alloc();
        if (!sw_frame) {
            LOG_OOM();
            return false;
        }
        int ret = av_hwframe_transfer_data(sw_frame, frame, 0);
        if (ret < 0) {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            LOGE("Could not transfer hardware frame to system memory: %s",
                 err);
            av_frame_free(&sw_frame);
            return false;
        }
        ret = av_frame_copy_props(sw_frame, frame);
        if (ret < 0) {
            LOGE("Could not copy hardware frame properties: %d", ret);
            av_frame_free(&sw_frame);
            return false;
        }

        // A hardware texture may be static or backed by a decoder surface.
        // Once rendering has fallen back, keep reusing the software texture.
        if (tex->texture && tex->texture_hardware) {
            sc_texture_destroy_texture(tex);
        }
        bool ok = sc_texture_set_from_sw_frame(tex, sw_frame);
        av_frame_free(&sw_frame);
        return ok;
    }

    if (tex->texture && tex->texture_hardware) {
        if (!sc_hwaccel_prepare_software(&tex->hwaccel)) {
            LOGE("Could not restore SDL's software texture configuration");
            return false;
        }
        sc_texture_destroy_texture(tex);
    }
#endif

    return sc_texture_set_from_sw_frame(tex, frame);
}

bool
sc_texture_set_from_surface(struct sc_texture *tex, SDL_Surface *surface) {
    sc_texture_destroy_texture(tex);

    tex->texture = SDL_CreateTextureFromSurface(tex->renderer, surface);
    if (!tex->texture) {
        LOGE("Could not create texture: %s", SDL_GetError());
        return false;
    }

    tex->texture_size.width = surface->w;
    tex->texture_size.height = surface->h;
    tex->texture_type = SC_TEXTURE_TYPE_ICON;
    tex->texture_format = surface->format;
    tex->texture_hardware = false;

    return true;
}

void
sc_texture_reset(struct sc_texture *tex) {
    sc_texture_destroy_texture(tex);
}
