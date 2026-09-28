#include "interop_software.h"

#include <assert.h>

#include <SDL3/SDL.h>

#include "opengl.h"
#include "util/log.h"

/** Downcast interop to sc_interop_software */
#define DOWNCAST(INTEROP) \
    container_of(INTEROP, struct sc_interop_software, interop)

static bool
sc_interop_software_create_texture(struct sc_interop_software *sw,
                                   struct sc_size size,
                                   SDL_Colorspace colorspace) {
    LOGV("Creating new texture: size=%" PRIu16 "x%" PRIu16 " color_space=%d ",
         size.width, size.height, colorspace);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        LOG_OOM();
        return false;
    }

    bool ok =
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              SDL_PIXELFORMAT_YV12);
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

    SDL_Texture *texture = SDL_CreateTextureWithProperties(sw->renderer, props);
    SDL_DestroyProperties(props);
    if (!texture) {
        LOGD("Could not create texture: %s", SDL_GetError());
        return false;
    }

    if (sw->mipmaps) {
        // The properties are owned by the texture
        SDL_PropertiesID props = SDL_GetTextureProperties(texture);
        if (!props) {
            LOGE("Could not get texture properties: %s", SDL_GetError());
            SDL_DestroyTexture(texture);
            return false;
        }

        // A YV12 texture is backed by one OpenGL texture per plane
        static const char *const opengl_keys[3] = {
            SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER,
            SDL_PROP_TEXTURE_OPENGL_TEXTURE_U_NUMBER,
            SDL_PROP_TEXTURE_OPENGL_TEXTURE_V_NUMBER,
        };
        static const char *const opengles2_keys[3] = {
            SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER,
            SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_U_NUMBER,
            SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_V_NUMBER,
        };

        const char *const *keys = sw->gl->is_opengles ? opengles2_keys
                                                      : opengl_keys;

        for (unsigned i = 0; i < 3; ++i) {
            int64_t texture_id = SDL_GetNumberProperty(props, keys[i], 0);
            if (!texture_id) {
                LOGE("Could not get texture id: %s", SDL_GetError());
                SDL_DestroyTexture(texture);
                return false;
            }

            assert(!(texture_id & ~0xFFFFFFFF)); // fits in uint32_t
            sw->texture_ids[i] = texture_id;
        }

        sc_opengl_enable_mipmaps(sw->gl, sw->texture_ids, 3);
    }

    sw->interop.texture = texture;
    sw->interop.frame_size = size;
    LOGI("Texture: %" PRIu16 "x%" PRIu16, size.width, size.height);

    return true;
}

static void
sc_interop_software_reset(struct sc_interop *interop) {
    if (interop->texture) {
        SDL_DestroyTexture(interop->texture);
        interop->texture = NULL;
    }
}

static void
sc_interop_software_destroy(struct sc_interop *interop) {
    if (interop->texture) {
        SDL_DestroyTexture(interop->texture);
    }
}

static bool
sc_interop_software_import(struct sc_interop *interop, const AVFrame *frame) {
    struct sc_interop_software *sw = DOWNCAST(interop);

    struct sc_size size = {frame->width, frame->height};
    assert(size.width && size.height);

    if (!interop->texture
            || interop->frame_size.width != size.width
            || interop->frame_size.height != size.height) {
        // Incompatible texture, recreate it
        sc_interop_software_reset(interop);

        SDL_Colorspace colorspace =
            sc_interop_to_sdl_color_space(frame->colorspace,
                                          frame->color_range);
        bool ok = sc_interop_software_create_texture(sw, size, colorspace);
        if (!ok) {
            return false;
        }
    }

    assert(interop->texture);

    bool ok = SDL_UpdateYUVTexture(interop->texture, NULL,
                                   frame->data[0], frame->linesize[0],
                                   frame->data[1], frame->linesize[1],
                                   frame->data[2], frame->linesize[2]);
    if (!ok) {
        LOGD("Could not update texture: %s", SDL_GetError());
        return false;
    }

    if (sw->mipmaps) {
        sc_opengl_generate_mipmaps(sw->gl, sw->texture_ids, 3);
    }

    return true;
}

struct sc_interop_software *
sc_interop_software_new(SDL_Renderer *renderer, struct sc_opengl *gl,
                        bool mipmaps) {
    assert(!mipmaps || gl); // mipmaps implies gl

    struct sc_interop_software *sw = malloc(sizeof(*sw));
    if (!sw) {
        LOG_OOM();
        return NULL;
    }

    static const struct sc_interop_ops ops = {
        .import = sc_interop_software_import,
        .reset = sc_interop_software_reset,
        .destroy = sc_interop_software_destroy,
    };

    sw->interop.name = "software";
    sw->interop.pix_fmt = AV_PIX_FMT_YUV420P;
    sw->interop.texture = NULL;
    sw->interop.ops = &ops;

    sw->renderer = renderer;
    sw->gl = gl;
    sw->mipmaps = mipmaps;

    return sw;
}
