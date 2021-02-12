#include "frame_texture.h"

#include <assert.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "util/log.h"

static inline bool
is_swscale_enabled(enum sc_scale_filter scale_filter) {
    return scale_filter != SC_SCALE_FILTER_NONE
        && scale_filter != SC_SCALE_FILTER_TRILINEAR;
}

static SDL_Texture *
create_texture(struct sc_frame_texture *ftex, struct size size) {
    SDL_Renderer *renderer = ftex->renderer;
    SDL_PixelFormatEnum fmt = is_swscale_enabled(ftex->scale_filter)
                            ? SDL_PIXELFORMAT_RGB24
                            : SDL_PIXELFORMAT_YV12;
    SDL_Texture *texture = SDL_CreateTexture(renderer, fmt,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             size.width, size.height);
    if (!texture) {
        return NULL;
    }

    if (ftex->scale_filter == SC_SCALE_FILTER_TRILINEAR) {
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
                      enum sc_scale_filter scale_filter,
                      struct size initial_size) {
    SDL_RendererInfo renderer_info;
    int r = SDL_GetRendererInfo(renderer, &renderer_info);
    const char *renderer_name = r ? NULL : renderer_info.name;
    LOGI("Renderer: %s", renderer_name ? renderer_name : "(unknown)");

    ftex->scale_filter = scale_filter;

    // starts with "opengl"
    bool use_opengl = renderer_name && !strncmp(renderer_name, "opengl", 6);
    if (use_opengl) {
        struct sc_opengl *gl = &ftex->gl;
        sc_opengl_init(gl);

        LOGI("OpenGL version: %s", gl->version);

        if (scale_filter == SC_SCALE_FILTER_TRILINEAR) {
            bool supports_mipmaps =
                sc_opengl_version_at_least(gl, 3, 0, /* OpenGL 3.0+ */
                                               2, 0  /* OpenGL ES 2.0+ */);
            if (!supports_mipmaps) {
                LOGW("Trilinear filtering disabled "
                     "(OpenGL 3.0+ or ES 2.0+ required)");
                ftex->scale_filter = SC_SCALE_FILTER_NONE;
            }
        }
    } else if (scale_filter == SC_SCALE_FILTER_TRILINEAR) {
        LOGD("Trilinear filtering disabled (not an OpenGL renderer)");
    }

    LOGD("Scale filter: %s\n", sc_scale_filter_name(ftex->scale_filter));

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
    ftex->decoded_frame = NULL;
    memset(ftex->data, 0, sizeof(ftex->data));
    memset(ftex->linesize, 0, sizeof(ftex->linesize));

    return true;
}

void
sc_frame_texture_destroy(struct sc_frame_texture *ftex) {
    if (ftex->texture) {
        SDL_DestroyTexture(ftex->texture);
    }
}

static int
to_sws_filter(enum sc_scale_filter scale_filter) {
    switch (scale_filter) {
        case SC_SCALE_FILTER_BILINEAR:  return SWS_BILINEAR;
        case SC_SCALE_FILTER_BICUBIC:   return SWS_BICUBIC;
        case SC_SCALE_FILTER_X:         return SWS_X;
        case SC_SCALE_FILTER_POINT:     return SWS_POINT;
        case SC_SCALE_FILTER_AREA:      return SWS_AREA;
        case SC_SCALE_FILTER_BICUBLIN:  return SWS_BICUBLIN;
        case SC_SCALE_FILTER_GAUSS:     return SWS_GAUSS;
        case SC_SCALE_FILTER_SINC:      return SWS_SINC;
        case SC_SCALE_FILTER_LANCZOS:   return SWS_LANCZOS;
        case SC_SCALE_FILTER_SPLINE:    return SWS_SPLINE;
        default: assert(!"unsupported filter");
    }
}

static bool
screen_generate_resized_frame(struct sc_frame_texture *ftex,
                              struct size target_size) {
    assert(is_swscale_enabled(ftex->scale_filter));
    // TODO

    if (ftex->data[0]) {
        av_freep(&ftex->data[0]);
    }

    int ret = av_image_alloc(ftex->data, ftex->linesize, target_size.width,
                             target_size.height, AV_PIX_FMT_RGB24, 16);
    if (ret < 0) {
        return false;
    }

    const AVFrame *input = ftex->decoded_frame;

    int flags = to_sws_filter(ftex->scale_filter);
    struct SwsContext *ctx =
        sws_getContext(input->width, input->height, AV_PIX_FMT_YUV420P,
                       target_size.width, target_size.height, AV_PIX_FMT_RGB24,
                       flags, NULL, NULL, NULL);
    if (!ctx) {
        return false;
    }

    sws_scale(ctx, (const uint8_t *const *) input->data, input->linesize, 0,
              input->height, ftex->data, ftex->linesize);
    sws_freeContext(ctx);

    return true;
}

static bool
sc_frame_texture_update_direct(struct sc_frame_texture *ftex,
                               const AVFrame *frame) {
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

    if (ftex->scale_filter == SC_SCALE_FILTER_TRILINEAR) {
        SDL_GL_BindTexture(ftex->texture, NULL, NULL);
        ftex->gl.GenerateMipmap(GL_TEXTURE_2D);
        SDL_GL_UnbindTexture(ftex->texture);
    }

    return true;
}

static bool
sc_frame_texture_update_swscale(struct sc_frame_texture *ftex,
                                const AVFrame *frame, struct size target_size) {
    if (ftex->texture_size.width != target_size.width
            || ftex->texture_size.height != target_size.height) {
        // Frame dimensions changed, destroy texture
        SDL_DestroyTexture(ftex->texture);

        ftex->texture = create_texture(ftex, target_size);
        if (!ftex->texture) {
            LOGC("Could not create texture: %s", SDL_GetError());
            return false;
        }

        ftex->texture_size = target_size;
    }

    // The frame is valid until the next call to
    // video_buffer_take_rendering_frame()
    ftex->decoded_frame = frame;

    bool ok = screen_generate_resized_frame(ftex, target_size);
    if (!ok) {
        LOGE("Failed to resize frame\n");
        return false;
    }

    SDL_UpdateTexture(ftex->texture, NULL, ftex->data[0], ftex->linesize[0]);

    if (ftex->scale_filter == SC_SCALE_FILTER_TRILINEAR) {
        SDL_GL_BindTexture(ftex->texture, NULL, NULL);
        ftex->gl.GenerateMipmap(GL_TEXTURE_2D);
        SDL_GL_UnbindTexture(ftex->texture);
    }

    return true;
}

bool
sc_frame_texture_update(struct sc_frame_texture *ftex, const AVFrame *frame,
                        struct size target_size) {
    if (is_swscale_enabled(ftex->scale_filter)) {
        return sc_frame_texture_update_swscale(ftex, frame, target_size);
    }
    return sc_frame_texture_update_direct(ftex, frame);
}
