#include "frame_texture.h"

#include "util/log.h"

static SDL_Texture *
create_texture(struct sc_frame_texture *ftex, struct size size) {
    SDL_Renderer *renderer = ftex->renderer;
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             size.width, size.height);
    if (!texture) {
        return NULL;
    }

    if (ftex->mipmaps) {
        struct sc_opengl *gl = &ftex->gl;

        SDL_GL_BindTexture(texture, NULL, NULL);

        // Enable trilinear filtering for downscaling
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          GL_LINEAR_MIPMAP_LINEAR);
        gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.f);

        SDL_GL_UnbindTexture(texture);
    }

    return texture;
}

bool
sc_frame_texture_init(struct sc_frame_texture *ftex, SDL_Renderer *renderer,
                      bool mipmaps, struct size initial_size) {
    SDL_RendererInfo renderer_info;
    int r = SDL_GetRendererInfo(renderer, &renderer_info);
    const char *renderer_name = r ? NULL : renderer_info.name;
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    ftex->mipmaps = false;

    // starts with "opengl"
    bool use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (use_opengl) {
        struct sc_opengl *gl = &ftex->gl;
        sc_opengl_init(gl);

        LOGI("OpenGL version: %s", gl->version);

        if (mipmaps) {
            bool supports_mipmaps =
                sc_opengl_version_at_least(gl, 3, 0, /* OpenGL 3.0+ */
                                               2, 0  /* OpenGL ES 2.0+ */);
            if (supports_mipmaps) {
                LOGI("Trilinear filtering enabled");
                ftex->mipmaps = true;
            } else {
                LOGW("Trilinear filtering disabled "
                     "(OpenGL 3.0+ or ES 2.0+ required)");
            }
        } else {
            LOGI("Trilinear filtering disabled");
        }
    } else {
        LOGD("Trilinear filtering disabled (not an OpenGL renderer)");
    }

    LOGI("Initial texture: %" PRIu16 "x%" PRIu16, initial_size.width,
                                                  initial_size.height);
    ftex->renderer = renderer;
    ftex->texture = create_texture(ftex, initial_size);
    if (!ftex->texture) {
        LOGC("Could not create texture: %s", SDL_GetError());
        SDL_DestroyRenderer(ftex->renderer);
        return false;
    }

    ftex->texture_size = initial_size;

    return true;
}

void
sc_frame_texture_destroy(struct sc_frame_texture *ftex) {
    if (ftex->texture) {
        SDL_DestroyTexture(ftex->texture);
    }
}

bool
sc_frame_texture_update(struct sc_frame_texture *ftex, const AVFrame *frame) {
    struct size frame_size = {frame->width, frame->height};
    if (ftex->texture_size.width != frame_size.width
            || ftex->texture_size.height != frame_size.height) {
        // Frame dimensions changed, destroy texture
        SDL_DestroyTexture(ftex->texture);

        LOGI("New texture: %" PRIu16 "x%" PRIu16, frame_size.width,
                                                  frame_size.height);
        ftex->texture = create_texture(ftex, frame_size);
        if (!ftex->texture) {
            LOGC("Could not create texture: %s", SDL_GetError());
            return false;
        }

        ftex->texture_size = frame_size;
    }

    SDL_UpdateYUVTexture(ftex->texture, NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);

    if (ftex->mipmaps) {
        SDL_GL_BindTexture(ftex->texture, NULL, NULL);
        ftex->gl.GenerateMipmap(GL_TEXTURE_2D);
        SDL_GL_UnbindTexture(ftex->texture);
    }

    return true;
}
