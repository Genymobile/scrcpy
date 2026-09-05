#include "texture.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <libavutil/pixfmt.h>

#include "util/log.h"

bool
sc_texture_init(struct sc_texture *tex, SDL_Renderer *renderer, bool mipmaps) {
    const char *renderer_name = SDL_GetRendererName(renderer);
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    tex->mipmaps = false;
    tex->pix_fmt = AV_PIX_FMT_NONE;

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
    return true;
}

void
sc_texture_destroy(struct sc_texture *tex) {
    if (tex->texture) {
        SDL_DestroyTexture(tex->texture);
    }
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

static SDL_PixelFormat
sc_texture_to_sdl_format(int pix_fmt) {
    switch (pix_fmt) {
        case AV_PIX_FMT_VIDEOTOOLBOX:
            // VideoToolbox decodes to a bi-planar YUV 4:2:0 CVPixelBuffer,
            // equivalent to NV12 layout. The pixel format value is passed to
            // SDL_CreateTextureWithProperties for Metal PIXELBUFFER wrapping;
            // the Metal backend uses it to interpret the buffer planes.
            return SDL_PIXELFORMAT_NV12;
        case AV_PIX_FMT_YUV420P:
            return SDL_PIXELFORMAT_YV12;
        default:
            // Reaching here means a new pixel format was added without
            // updating this function. In debug builds the upstream assert
            // in sc_screen_frame_sink_open catches this earlier.
            LOGE("Unsupported pixel format: %d", pix_fmt);
            assert(!"Unsupported pixel format");
            return SDL_PIXELFORMAT_YV12;
    }
}

static SDL_TextureAccess
sc_texture_to_sdl_access(int pix_fmt) {
    switch (pix_fmt) {
        case AV_PIX_FMT_YUV420P:
            return SDL_TEXTUREACCESS_STREAMING;
        case AV_PIX_FMT_VIDEOTOOLBOX:
            return SDL_TEXTUREACCESS_STATIC;
        default:
            LOGE("Unsupported pixel format: %d", pix_fmt);
            assert(!"Unsupported pixel format");
            return SDL_TEXTUREACCESS_STREAMING;
    }
}

static SDL_Texture *
sc_texture_create_frame_texture(struct sc_texture *tex,
                                struct sc_size size,
                                enum AVColorSpace color_space,
                                enum AVColorRange color_range,
                                int pix_fmt,
                                void *pixelbuffer) {
    LOGV("Creating new texture: size=%" PRIu16 "x%" PRIu16 " color_space=%d "
         "color_range=%d pix_fmt=%d pixelbuffer=%p",
         size.width, size.height, color_space, color_range, pix_fmt,
         pixelbuffer);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return NULL;
    }

    enum SDL_Colorspace sdl_color_space =
        sc_texture_to_sdl_color_space(color_space, color_range);

    SDL_PixelFormat sdl_format = sc_texture_to_sdl_format(pix_fmt);
    SDL_TextureAccess sdl_access = sc_texture_to_sdl_access(pix_fmt);

    bool ok =
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              sdl_format);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                                sdl_access);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,
                                size.width);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,
                                size.height);
    ok &= SDL_SetNumberProperty(props,
                                SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,
                                sdl_color_space);

    // Attach CVPixelBuffer for zero-copy Metal texture wrapping.
    // The Metal framework retains the CVPixelBuffer when creating the
    // texture, so the buffer stays valid until SDL_DestroyTexture.
    // The AVFrame (via av_frame_ref in frame_buffer) also holds a
    // reference during the current frame's render cycle.
    if (pixelbuffer) {
        ok &= SDL_SetPointerProperty(props,
                                     SDL_PROP_TEXTURE_CREATE_METAL_PIXELBUFFER_POINTER,
                                     pixelbuffer);
    }

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

    // Mipmaps are only supported for OpenGL streaming textures, not for
    // wrapped pixel buffer textures.
    if (tex->mipmaps && !pixelbuffer) {
        struct sc_opengl *gl = &tex->gl;

        SDL_PropertiesID props = SDL_GetTextureProperties(texture);
        if (!props) {
            LOGE("Could not get texture properties: %s", SDL_GetError());
            SDL_DestroyTexture(texture);
            return NULL;
        }

        const char *renderer_name = SDL_GetRendererName(tex->renderer);
        const char *key = !renderer_name || !strcmp(renderer_name, "opengl")
                        ? SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER
                        : SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER;

        int64_t texture_id = SDL_GetNumberProperty(props, key, 0);
        SDL_DestroyProperties(props);
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

bool
sc_texture_set_from_frame(struct sc_texture *tex, const AVFrame *frame) {

    struct sc_size size = {frame->width, frame->height};
    assert(size.width && size.height);

    // VideoToolbox zero-copy path: extract the CVPixelBufferRef from the
    // hardware frame and wrap it as a Metal texture. A new texture is
    // created every frame since each frame carries a different pixel buffer.
    //
    // A LOGI is emitted only on size change; logging on every frame would
    // be verbose since more than 99% of frames reuse the same size.
    if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
        assert(frame->data[3]);

        bool size_changed = tex->texture_size.width != size.width
            || tex->texture_size.height != size.height;

        // Always destroy the old texture — the CVPixelBuffer it wraps is
        // no longer valid.
        if (tex->texture) {
            SDL_DestroyTexture(tex->texture);
            tex->texture = NULL;
        }

        tex->texture = sc_texture_create_frame_texture(tex, size,
                                                       frame->colorspace,
                                                       frame->color_range,
                                                       frame->format,
                                                       (void *) frame->data[3]);
        if (!tex->texture) {
            tex->texture_size = (struct sc_size) {0, 0};
            tex->texture_type = SC_TEXTURE_TYPE_FRAME;
            tex->pix_fmt = AV_PIX_FMT_NONE;
            return false;
        }

        tex->texture_size = size;
        tex->texture_type = SC_TEXTURE_TYPE_FRAME;
        tex->pix_fmt = frame->format;

        if (size_changed) {
            LOGI("Texture: %" PRIu16 "x%" PRIu16 " pix_fmt=%d", size.width,
                 size.height, frame->format);
        }
        return true;
    }

    // Software/copy path: update a streaming texture from CPU pixel data.
    if (!tex->texture
            || tex->texture_type != SC_TEXTURE_TYPE_FRAME
            || tex->texture_size.width != size.width
            || tex->texture_size.height != size.height
            || tex->pix_fmt != frame->format) {
        // Incompatible texture, recreate it
        enum AVColorSpace color_space = frame->colorspace;
        enum AVColorRange color_range = frame->color_range;

        if (tex->texture) {
            SDL_DestroyTexture(tex->texture);
        }

        tex->texture = sc_texture_create_frame_texture(tex, size, color_space,
                                                       color_range,
                                                       frame->format,
                                                       NULL);
        if (!tex->texture) {
            tex->texture_size = (struct sc_size) {0, 0};
            tex->texture_type = SC_TEXTURE_TYPE_FRAME;
            tex->pix_fmt = AV_PIX_FMT_NONE;
            return false;
        }

        tex->texture_size = size;
        tex->texture_type = SC_TEXTURE_TYPE_FRAME;
        tex->pix_fmt = frame->format;

        LOGI("Texture: %" PRIu16 "x%" PRIu16, size.width, size.height);
    }

    assert(tex->texture);
    assert(tex->texture_type == SC_TEXTURE_TYPE_FRAME);

    bool ok = SDL_UpdateYUVTexture(tex->texture, NULL,
                                   frame->data[0], frame->linesize[0],
                                   frame->data[1], frame->linesize[1],
                                   frame->data[2], frame->linesize[2]);
    if (!ok) {
        LOGD("Could not update texture: %s", SDL_GetError());
        return false;
    }

    if (tex->mipmaps) {
        assert(tex->texture_id);
        struct sc_opengl *gl = &tex->gl;

        gl->BindTexture(GL_TEXTURE_2D, tex->texture_id);
        gl->GenerateMipmap(GL_TEXTURE_2D);
        gl->BindTexture(GL_TEXTURE_2D, 0);
    }

    return true;
}

bool
sc_texture_set_from_surface(struct sc_texture *tex, SDL_Surface *surface) {
    if (tex->texture) {
        SDL_DestroyTexture(tex->texture);
    }

    tex->pix_fmt = AV_PIX_FMT_NONE;
    tex->texture = SDL_CreateTextureFromSurface(tex->renderer, surface);
    if (!tex->texture) {
        LOGE("Could not create texture: %s", SDL_GetError());
        // Reset state to avoid inconsistency
        tex->texture_size = (struct sc_size) {0, 0};
        tex->texture_type = SC_TEXTURE_TYPE_ICON;
        return false;
    }

    tex->texture_size.width = surface->w;
    tex->texture_size.height = surface->h;
    tex->texture_type = SC_TEXTURE_TYPE_ICON;

    return true;
}

void
sc_texture_reset(struct sc_texture *tex) {
    if (tex->texture) {
        SDL_DestroyTexture(tex->texture);
        tex->texture = NULL;
    }
    tex->pix_fmt = AV_PIX_FMT_NONE;
}
