#include "display.h"

#include <assert.h>

#include "util/log.h"

bool
sc_display_init(struct sc_display *display, SDL_Window *window, bool mipmaps) {
    display->renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!display->renderer) {
        LOGE("Could not create renderer: %s", SDL_GetError());
        return false;
    }

    SDL_RendererInfo renderer_info;
    int r = SDL_GetRendererInfo(display->renderer, &renderer_info);
    const char *renderer_name = r ? NULL : renderer_info.name;
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    display->mipmaps = false;

    // starts with "opengl"
    bool use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (use_opengl) {
        struct sc_opengl *gl = &display->gl;
        sc_opengl_init(gl);

        LOGI("OpenGL version: %s", gl->version);

        if (mipmaps) {
            bool supports_mipmaps =
                sc_opengl_version_at_least(gl, 3, 0, /* OpenGL 3.0+ */
                                               2, 0  /* OpenGL ES 2.0+ */);
            if (supports_mipmaps) {
                LOGI("Trilinear filtering enabled");
                display->mipmaps = true;
            } else {
                LOGW("Trilinear filtering disabled "
                     "(OpenGL 3.0+ or ES 2.0+ required");
            }
        } else {
            LOGI("Trilinear filtering disabled");
        }
    } else if (mipmaps) {
        LOGD("Trilinear filtering disabled (not an OpenGL renderer");
    }

    return true;
}

void
sc_display_destroy(struct sc_display *display) {
    if (display->texture) {
        SDL_DestroyTexture(display->texture);
    }
    SDL_DestroyRenderer(display->renderer);
}

static SDL_Texture *
sc_display_create_texture(struct sc_display *display,
                          struct sc_size size) {
    SDL_Renderer *renderer = display->renderer;
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             size.width, size.height);
    if (!texture) {
        LOGE("Could not create texture: %s", SDL_GetError());
        return NULL;
    }

    if (display->mipmaps) {
        struct sc_opengl *gl = &display->gl;

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
sc_display_set_texture_size(struct sc_display *display, struct sc_size size) {
    if (display->texture) {
        SDL_DestroyTexture(display->texture);
    }

    display->texture = sc_display_create_texture(display, size);
    if (!display->texture) {
        return false;
    }

    LOGI("Texture: %" PRIu16 "x%" PRIu16, size.width, size.height);
    return true;
}

bool
sc_display_update_texture(struct sc_display *display, const AVFrame *frame) {
    int ret = SDL_UpdateYUVTexture(display->texture, NULL,
                                   frame->data[0], frame->linesize[0],
                                   frame->data[1], frame->linesize[1],
                                   frame->data[2], frame->linesize[2]);
    if (ret) {
        LOGE("Could not update texture: %s", SDL_GetError());
        return false;
    }

    if (display->mipmaps) {
        SDL_GL_BindTexture(display->texture, NULL, NULL);
        display->gl.GenerateMipmap(GL_TEXTURE_2D);
        SDL_GL_UnbindTexture(display->texture);
    }

    return true;
}

bool
sc_display_render(struct sc_display *display, const SDL_Rect *geometry,
                  unsigned rotation) {
    SDL_RenderClear(display->renderer);

    SDL_Renderer *renderer = display->renderer;
    SDL_Texture *texture = display->texture;

    if (rotation == 0) {
        int ret = SDL_RenderCopy(renderer, texture, NULL, geometry);
        if (ret) {
            LOGE("Could not render texture: %s", SDL_GetError());
            return false;
        }
    } else {
        // rotation in RenderCopyEx() is clockwise, while screen->rotation is
        // counterclockwise (to be consistent with --lock-video-orientation)
        int cw_rotation = (4 - rotation) % 4;
        double angle = 90 * cw_rotation;

        const SDL_Rect *dstrect = NULL;
        SDL_Rect rect;
        if (rotation & 1) {
            rect.x = geometry->x + (geometry->w - geometry->h) / 2;
            rect.y = geometry->y + (geometry->h - geometry->w) / 2;
            rect.w = geometry->h;
            rect.h = geometry->w;
            dstrect = &rect;
        } else {
            assert(rotation == 2);
            dstrect = geometry;
        }

        int ret = SDL_RenderCopyEx(renderer, texture, NULL, dstrect, angle,
                                   NULL, 0);
        if (ret) {
            LOGE("Could not render texture: %s", SDL_GetError());
            return false;
        }
    }

    SDL_RenderPresent(display->renderer);
    return true;
}
