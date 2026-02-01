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
                                enum AVColorSpace color_space,
                                enum AVColorRange color_range) {
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return NULL;
    }

    enum SDL_Colorspace sdl_color_space =
        sc_texture_to_sdl_color_space(color_space, color_range);

    bool ok =
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              SDL_PIXELFORMAT_YV12);
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

    if (tex->mipmaps) {
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

    if (!tex->texture
            || tex->texture_type != SC_TEXTURE_TYPE_FRAME
            || tex->texture_size.width != size.width
            || tex->texture_size.height != size.height) {
        // Incompatible texture, recreate it
        enum AVColorSpace color_space = frame->colorspace;
        enum AVColorRange color_range = frame->color_range;

        if (tex->texture) {
            SDL_DestroyTexture(tex->texture);
        }

        tex->texture = sc_texture_create_frame_texture(tex, size, color_space,
                                                       color_range);
        if (!tex->texture) {
            return false;
        }

        tex->texture_size = size;
        tex->texture_type = SC_TEXTURE_TYPE_FRAME;

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

    tex->texture = SDL_CreateTextureFromSurface(tex->renderer, surface);
    if (!tex->texture) {
        LOGE("Could not create texture: %s", SDL_GetError());
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
}
