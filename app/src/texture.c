#include "texture.h"

#include <assert.h>
#include <string.h>

#include "util/log.h"

bool
sc_texture_init(struct sc_texture *tex, SDL_Renderer *renderer, bool mipmaps,
                enum AVHWDeviceType hw_type) {
    const char *renderer_name = SDL_GetRendererName(renderer);
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    tex->has_gl = false;
    tex->mipmaps = false;

    struct sc_opengl *gl = NULL;

    // starts with "opengl"
    bool use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (use_opengl) {
        gl = &tex->gl;
        sc_opengl_init(gl);
        tex->has_gl = true;

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
    tex->interop = sc_interop_new(hw_type, tex->renderer, gl, tex->mipmaps);
    if (!tex->interop) {
        return false;
    }

    return true;
}

void
sc_texture_destroy(struct sc_texture *tex) {
    sc_interop_delete(tex->interop);
}

SDL_Texture *
sc_texture_get(struct sc_texture *tex) {
    assert(tex->interop);
    return tex->interop->texture;
}

struct sc_size
sc_texture_get_frame_size(struct sc_texture *tex) {
    assert(tex->interop && tex->interop->texture);
    return tex->interop->frame_size;
}

bool
sc_texture_update(struct sc_texture *tex, const AVFrame *frame) {
    struct sc_interop *interop = tex->interop;
    if (!interop) {
        return false;
    }

    assert(interop->ops && interop->ops->import);
    return interop->ops->import(interop, frame);
}

void
sc_texture_reset(struct sc_texture *tex) {
    struct sc_interop *interop = tex->interop;
    if (interop) {
        assert(interop->ops && interop->ops->reset);
        interop->ops->reset(interop);
    }
}
