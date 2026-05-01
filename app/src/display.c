#include "display.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

// Render the overlay UI: checkboxes, labels, and info line
void sc_render_overlay_ui(struct sc_display *display) {
    // 8x12 font for ASCII 32..127 (printable)
    static const uint8_t font8x12[96][12] = {
        {0,0,0,0,0,0,0,0,0,0,0,0}, // (space)
        {24,24,24,24,24,24,24,0,24,24,0,0}, // !
        {54,54,54,36,0,0,0,0,0,0,0,0}, // "
        {0,36,36,126,36,36,36,126,36,36,0,0}, // #
        {8,62,73,72,62,9,73,62,8,8,0,0}, // $
        {0,99,147,102,12,24,48,102,201,198,0,0}, // %
        {28,34,34,20,40,82,73,70,61,0,0,0}, // &
        {24,24,24,16,0,0,0,0,0,0,0,0}, // '
        {12,24,48,48,48,48,48,48,24,12,0,0}, // (
        {48,24,12,12,12,12,12,12,24,48,0,0}, // )
        {0,0,36,24,126,24,36,0,0,0,0,0}, // *
        {0,0,24,24,126,24,24,0,0,0,0,0}, // +
        {0,0,0,0,0,0,0,24,24,16,0,0}, // ,
        {0,0,0,0,126,0,0,0,0,0,0,0}, // -
        {0,0,0,0,0,0,0,24,24,0,0,0}, // .
        {0,3,6,12,24,48,96,192,128,0,0,0}, // /
        {60,66,99,115,91,79,70,66,60,0,0,0}, // 0
        {24,56,24,24,24,24,24,24,126,0,0,0}, // 1
        {60,66,3,6,12,24,48,96,127,0,0,0}, // 2
        {60,66,3,6,28,6,3,66,60,0,0,0}, // 3
        {6,14,30,54,102,127,6,6,15,0,0,0}, // 4
        {127,64,64,124,66,3,3,66,60,0,0,0}, // 5
        {28,48,96,124,102,99,99,102,60,0,0,0}, // 6
        {127,3,6,12,24,48,48,48,48,0,0,0}, // 7
        {60,66,99,102,60,102,99,66,60,0,0,0}, // 8
        {60,102,99,99,63,6,12,24,56,0,0,0}, // 9
        {0,0,24,24,0,0,24,24,0,0,0,0}, // :
        {0,0,24,24,0,0,24,24,16,0,0,0}, // ;
        {12,24,48,96,192,96,48,24,12,0,0,0}, // <
        {0,0,126,0,126,0,0,0,0,0,0,0}, // =
        {48,24,12,6,3,6,12,24,48,0,0,0}, // >
        {60,66,3,6,12,24,24,0,24,24,0,0}, // ?
        {60,66,99,111,107,111,96,62,0,0,0,0}, // @
        {24,60,102,102,126,102,102,102,231,0,0,0}, // A
        {124,102,102,124,102,102,102,102,124,0,0,0}, // B
        {60,102,96,96,96,96,96,102,60,0,0,0}, // C
        {120,108,102,102,102,102,102,108,120,0,0,0}, // D
        {126,96,96,124,96,96,96,96,126,0,0,0}, // E
        {126,96,96,124,96,96,96,96,96,0,0,0}, // F
        {60,102,96,96,110,102,102,102,60,0,0,0}, // G
        {102,102,102,126,102,102,102,102,102,0,0,0}, // H
        {60,24,24,24,24,24,24,24,60,0,0,0}, // I
        {6,6,6,6,6,6,6,102,60,0,0,0}, // J
        {102,108,120,112,120,108,102,102,102,0,0,0}, // K
        {96,96,96,96,96,96,96,96,126,0,0,0}, // L
        {99,119,127,107,99,99,99,99,99,0,0,0}, // M
        {102,102,118,126,110,102,102,102,102,0,0,0}, // N
        {60,102,99,99,99,99,99,102,60,0,0,0}, // O
        {124,102,102,102,124,96,96,96,96,0,0,0}, // P
        {60,102,99,99,99,99,107,102,61,0,0,0}, // Q
        {124,102,102,102,124,108,102,102,102,0,0,0}, // R
        {60,102,96,60,6,3,99,102,60,0,0,0}, // S
        {126,24,24,24,24,24,24,24,24,0,0,0}, // T
        {102,102,102,102,102,102,102,102,60,0,0,0}, // U
        {99,99,99,99,99,99,54,28,8,0,0,0}, // V
        {99,99,99,99,99,107,127,119,99,0,0,0}, // W
        {99,99,54,28,8,28,54,99,99,0,0,0}, // X
        {99,99,99,54,28,8,8,8,8,0,0,0}, // Y
        {127,3,6,12,24,48,96,96,127,0,0,0}, // Z
        {60,48,48,48,48,48,48,48,60,0,0,0}, // [
    {0,192,96,48,24,12,6,3,0,0,0,0}, // backslash
        {60,12,12,12,12,12,12,12,60,0,0,0}, // ]
        {8,28,54,99,0,0,0,0,0,0,0,0}, // ^
        {0,0,0,0,0,0,0,0,0,0,255,0}, // _
        {24,24,24,12,0,0,0,0,0,0,0,0}, // `
        {0,0,60,6,62,102,102,102,59,0,0,0}, // a
        {96,96,124,102,102,102,102,102,124,0,0,0}, // b
        {0,0,60,102,96,96,96,102,60,0,0,0}, // c
        {6,6,62,102,102,102,102,102,62,0,0,0}, // d
        {0,0,60,102,126,96,96,102,60,0,0,0}, // e
        {28,54,48,120,48,48,48,48,120,0,0,0}, // f
        {0,0,62,102,102,102,62,6,102,60,0,0}, // g
        {96,96,124,102,102,102,102,102,102,0,0,0}, // h
        {24,0,56,24,24,24,24,24,60,0,0,0}, // i
        {6,0,6,6,6,6,6,102,102,60,0,0}, // j
        {96,96,102,108,120,120,108,102,102,0,0,0}, // k
        {56,24,24,24,24,24,24,24,60,0,0,0}, // l
        {0,0,102,127,107,107,99,99,99,0,0,0}, // m
        {0,0,124,102,102,102,102,102,102,0,0,0}, // n
        {0,0,60,102,102,102,102,102,60,0,0,0}, // o
        {0,0,124,102,102,102,124,96,96,240,0,0}, // p
        {0,0,62,102,102,102,62,6,6,15,0,0}, // q
        {0,0,124,102,96,96,96,96,240,0,0,0}, // r
        {0,0,62,96,60,6,6,102,60,0,0,0}, // s
        {16,16,124,16,16,16,16,16,14,0,0,0}, // t
        {0,0,102,102,102,102,102,102,62,0,0,0}, // u
        {0,0,102,102,102,102,102,60,24,0,0,0}, // v
        {0,0,99,99,99,107,127,54,54,0,0,0}, // w
        {0,0,102,102,60,24,60,102,102,0,0,0}, // x
        {0,0,102,102,102,62,6,102,60,0,0,0}, // y
        {0,0,126,12,24,48,96,96,126,0,0,0}, // z
        {12,24,24,24,48,24,24,24,12,0,0,0}, // {
        {24,24,24,24,24,24,24,24,24,0,0,0}, // |
        {48,24,24,24,12,24,24,24,48,0,0,0}, // }
        {0,0,0,51,102,204,0,0,0,0,0,0}, // ~
    };
    const int char_w = 8;
    const int char_h = 12;
        const int spacing = 2;
        const int scale = 2;
    // Compose overlay lines
    char line1[64], line2[64], line3[128];
    snprintf(line1, sizeof(line1), "%s Pinch Zoom on Scroll", display->overlay_pinch_zoom_enabled ? "[x]" : "[ ]");
    snprintf(line2, sizeof(line2), "%s Overlay Toggle", display->overlay_toggle_enabled ? "[x]" : "[ ]");
    snprintf(line3, sizeof(line3), "%s", display->overlay_text);
    // Layout: 2 checkboxes + 1 info line
    int lines = 3;
    int maxlen = (int)strlen(line1);
    if ((int)strlen(line2) > maxlen) maxlen = (int)strlen(line2);
    if ((int)strlen(line3) > maxlen) maxlen = (int)strlen(line3);
        int text_w = maxlen * (char_w * scale + spacing);
        int text_h = lines * (char_h * scale) + (lines - 1) * (2 * scale);
    SDL_SetRenderDrawBlendMode(display->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 200); // dark semi-transparent background
    SDL_Rect bg = { display->overlay_x, display->overlay_y, text_w + 8, text_h + 8 };
    SDL_RenderFillRect(display->renderer, &bg);
        SDL_SetRenderDrawColor(display->renderer, 255, 255, 255, 255); // white text for readability
    int x = display->overlay_x + 4;
    int y = display->overlay_y + 4;
    const char *linestr[3] = { line1, line2, line3 };
    for (int l = 0; l < lines; ++l) {
        const char *text = linestr[l];
        int len = (int)strlen(text);
        int xx = x;
        for (int i = 0; i < len; ++i) {
            unsigned char c = text[i];
            if (c < 32 || c > 127) {
                    xx += char_w * scale + spacing;
                continue;
            }
            const uint8_t *glyph = font8x12[c - 32];
            for (int row = 0; row < char_h; ++row) {
                uint8_t bits = glyph[row];
                for (int col = 0; col < char_w; ++col) {
                    if (bits & (1 << (7 - col))) {
                            SDL_Rect r = { xx + col * scale, y + row * scale,
                                           scale, scale };
                        SDL_RenderFillRect(display->renderer, &r);
                    }
                }
            }
                xx += char_w * scale + spacing;
        }
            y += char_h * scale + 2 * scale;
    }
    // Set overlay_w and overlay_h for click detection
        display->overlay_w = text_w + 8;
        display->overlay_h = text_h + 8;
}
#include <assert.h>
#include <inttypes.h>
#include <string.h>
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

#ifdef SC_DISPLAY_FORCE_OPENGL_CORE_PROFILE
    display->gl_context = NULL;
#endif

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

    av_frame_unref(display->pending.frame);
    int r = av_frame_ref(display->pending.frame, frame);
    if (r) {
        LOGE("Could not ref frame: %d", r);
        return false;
    }

    display->pending.flags |= SC_DISPLAY_PENDING_FLAG_FRAME;

    return true;
}

// Forward declaration
static bool
sc_display_update_texture_internal(struct sc_display *display,
                                   const AVFrame *frame);

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
        bool ok = sc_display_update_texture_internal(display,
                                                     display->pending.frame);
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

        SDL_RenderPresent(display->renderer);
        return SC_DISPLAY_RESULT_OK;
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

    // draw overlay UI (checkboxes, labels, info line) if requested
    if (display->overlay_enabled) {
        // Call the new overlay rendering function (to be implemented if not present)
        sc_render_overlay_ui(display);
    }
    SDL_RenderPresent(display->renderer);
    return SC_DISPLAY_RESULT_OK;
}