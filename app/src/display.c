#include "display.h"

#include <assert.h>
#include <libavutil/pixfmt.h>

#include "util/log.h"

static bool
sc_display_init_novideo_icon(struct sc_display *display,
                             SDL_Surface *icon_novideo) {
    assert(icon_novideo);

    if (SDL_RenderSetLogicalSize(display->renderer,
                                 icon_novideo->w, icon_novideo->h)) {
        LOGW("Could not set renderer logical size: %s", SDL_GetError());
        // don't fail
    }

    display->texture = SDL_CreateTextureFromSurface(display->renderer,
                                                    icon_novideo);
    if (!display->texture) {
        LOGE("Could not create texture: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool
sc_display_init(struct sc_display *display, SDL_Window *window,
                SDL_Surface *icon_novideo, bool mipmaps) {
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

#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
        // Persuade macOS to give us something better than OpenGL 2.1.
        // If we create a Core Profile context, we get the best OpenGL version.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_CORE);

        LOGD("Creating OpenGL Core Profile context");
        display->gl_context = SDL_GL_CreateContext(window);
        if (!display->gl_context) {
            LOGE("Could not create OpenGL context: %s", SDL_GetError());
            SDL_DestroyRenderer(display->renderer);
            return false;
        }
#endif

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
                     "(OpenGL 3.0+ or ES 2.0+ required)");
            }
        } else {
            LOGI("Trilinear filtering disabled");
        }
    } else if (mipmaps) {
        LOGD("Trilinear filtering disabled (not an OpenGL renderer)");
    }

    display->texture = NULL;
    display->pending.flags = 0;
    display->pending.frame = NULL;
    display->has_frame = false;

    if (icon_novideo) {
        // Without video, set a static scrcpy icon as window content
        bool ok = sc_display_init_novideo_icon(display, icon_novideo);
        if (!ok) {
#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
            SDL_GL_DeleteContext(display->gl_context);
#endif
            SDL_DestroyRenderer(display->renderer);
            return false;
        }
    }

    return true;
}

void
sc_display_destroy(struct sc_display *display) {
    if (display->pending.frame) {
        av_frame_free(&display->pending.frame);
    }
#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
    SDL_GL_DeleteContext(display->gl_context);
#endif
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
        LOGD("Could not create texture: %s", SDL_GetError());
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

static inline void
sc_display_set_pending_size(struct sc_display *display, struct sc_size size) {
    assert(!display->texture);
    display->pending.size = size;
    display->pending.flags |= SC_DISPLAY_PENDING_FLAG_SIZE;
}

static bool
sc_display_set_pending_frame(struct sc_display *display, const AVFrame *frame) {
    if (!display->pending.frame) {
        display->pending.frame = av_frame_alloc();
        if (!display->pending.frame) {
            LOG_OOM();
            return false;
        }
    }

    int r = av_frame_ref(display->pending.frame, frame);
    if (r) {
        LOGE("Could not ref frame: %d", r);
        return false;
    }

    display->pending.flags |= SC_DISPLAY_PENDING_FLAG_FRAME;

    return true;
}

static bool
sc_display_apply_pending(struct sc_display *display) {
    if (display->pending.flags & SC_DISPLAY_PENDING_FLAG_SIZE) {
        assert(!display->texture);
        display->texture =
            sc_display_create_texture(display, display->pending.size);
        if (!display->texture) {
            return false;
        }

        display->pending.flags &= ~SC_DISPLAY_PENDING_FLAG_SIZE;
    }

    if (display->pending.flags & SC_DISPLAY_PENDING_FLAG_FRAME) {
        assert(display->pending.frame);
        bool ok = sc_display_update_texture(display, display->pending.frame);
        if (!ok) {
            return false;
        }

        av_frame_unref(display->pending.frame);
        display->pending.flags &= ~SC_DISPLAY_PENDING_FLAG_FRAME;
    }

    return true;
}

static bool
sc_display_set_texture_size_internal(struct sc_display *display,
                                     struct sc_size size) {
    assert(size.width && size.height);

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

enum sc_display_result
sc_display_set_texture_size(struct sc_display *display, struct sc_size size) {
    bool ok = sc_display_set_texture_size_internal(display, size);
    if (!ok) {
        sc_display_set_pending_size(display, size);
        return SC_DISPLAY_RESULT_PENDING;

    }

    return SC_DISPLAY_RESULT_OK;
}

static SDL_YUV_CONVERSION_MODE
sc_display_to_sdl_color_range(enum AVColorRange color_range) {
    return color_range == AVCOL_RANGE_JPEG ? SDL_YUV_CONVERSION_JPEG
                                           : SDL_YUV_CONVERSION_AUTOMATIC;
}

static bool
sc_display_update_texture_internal(struct sc_display *display,
                                   const AVFrame *frame) {
    if (!display->has_frame) {
        // First frame
        display->has_frame = true;

        // Configure YUV color range conversion
        SDL_YUV_CONVERSION_MODE sdl_color_range =
            sc_display_to_sdl_color_range(frame->color_range);
        SDL_SetYUVConversionMode(sdl_color_range);
    }

    int ret = SDL_UpdateYUVTexture(display->texture, NULL,
                                   frame->data[0], frame->linesize[0],
                                   frame->data[1], frame->linesize[1],
                                   frame->data[2], frame->linesize[2]);
    if (ret) {
        LOGD("Could not update texture: %s", SDL_GetError());
        return false;
    }

    if (display->mipmaps) {
        SDL_GL_BindTexture(display->texture, NULL, NULL);
        display->gl.GenerateMipmap(GL_TEXTURE_2D);
        SDL_GL_UnbindTexture(display->texture);
    }

    return true;
}

enum sc_display_result
sc_display_update_texture(struct sc_display *display, const AVFrame *frame) {
    bool ok = sc_display_update_texture_internal(display, frame);
    if (!ok) {
        ok = sc_display_set_pending_frame(display, frame);
        if (!ok) {
            LOGE("Could not set pending frame");
            return SC_DISPLAY_RESULT_ERROR;
        }

        return SC_DISPLAY_RESULT_PENDING;
    }

    return SC_DISPLAY_RESULT_OK;
}

enum sc_display_result
sc_display_render(struct sc_display *display, const SDL_Rect *geometry,
                  enum sc_orientation orientation) {
    SDL_RenderClear(display->renderer);

    if (display->pending.flags) {
        bool ok = sc_display_apply_pending(display);
        if (!ok) {
            return SC_DISPLAY_RESULT_PENDING;
        }
    }

    SDL_Renderer *renderer = display->renderer;
    SDL_Texture *texture = display->texture;

    if (orientation == SC_ORIENTATION_0) {
        int ret = SDL_RenderCopy(renderer, texture, NULL, geometry);
        if (ret) {
            LOGE("Could not render texture: %s", SDL_GetError());
            return SC_DISPLAY_RESULT_ERROR;
        }
    } else {
        unsigned cw_rotation = sc_orientation_get_rotation(orientation);
        double angle = 90 * cw_rotation;

        const SDL_Rect *dstrect = NULL;
        SDL_Rect rect;
        if (sc_orientation_is_swap(orientation)) {
            rect.x = geometry->x + (geometry->w - geometry->h) / 2;
            rect.y = geometry->y + (geometry->h - geometry->w) / 2;
            rect.w = geometry->h;
            rect.h = geometry->w;
            dstrect = &rect;
        } else {
            dstrect = geometry;
        }

        SDL_RendererFlip flip = sc_orientation_is_mirror(orientation)
                              ? SDL_FLIP_HORIZONTAL : 0;

        int ret = SDL_RenderCopyEx(renderer, texture, NULL, dstrect, angle,
                                   NULL, flip);
        if (ret) {
            LOGE("Could not render texture: %s", SDL_GetError());
            return SC_DISPLAY_RESULT_ERROR;
        }
    }

    SDL_RenderPresent(display->renderer);
    return SC_DISPLAY_RESULT_OK;
}
