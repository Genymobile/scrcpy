#include "display.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <libavutil/pixfmt.h>

#include "util/log.h"

// -------- Mini 5x7 bitmap font for toolbar button labels --------
//
// Each glyph occupies 5 columns × 7 rows. Rows are encoded low-5-bits,
// bit 4 = leftmost pixel.

struct sc_mini_glyph {
    char c;
    uint8_t rows[7];
};

static const struct sc_mini_glyph SC_MINI_FONT[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
};

static const uint8_t *
sc_mini_font_lookup(char c) {
    for (size_t i = 0;
         i < sizeof(SC_MINI_FONT) / sizeof(SC_MINI_FONT[0]); ++i) {
        if (SC_MINI_FONT[i].c == c) {
            return SC_MINI_FONT[i].rows;
        }
    }
    return NULL;
}

// Width in pixels of a string rendered at the given scale (1px gap between
// glyphs).
static int
sc_mini_text_width(const char *s, int scale) {
    int n = 0;
    for (const char *p = s; *p; ++p) {
        ++n;
    }
    if (n == 0) {
        return 0;
    }
    return (n * 5 + (n - 1)) * scale;
}

static void
sc_mini_draw_text(SDL_Renderer *r, int x, int y, int scale, const char *s) {
    int cx = x;
    for (const char *p = s; *p; ++p) {
        const uint8_t *glyph = sc_mini_font_lookup(*p);
        if (glyph) {
            for (int row = 0; row < 7; ++row) {
                uint8_t bits = glyph[row];
                for (int col = 0; col < 5; ++col) {
                    if (bits & (1 << (4 - col))) {
                        SDL_Rect px = {
                            .x = cx + col * scale,
                            .y = y + row * scale,
                            .w = scale,
                            .h = scale,
                        };
                        SDL_RenderFillRect(r, &px);
                    }
                }
            }
        }
        cx += 6 * scale; // 5 px glyph + 1 px gap
    }
}

// Draw a label centered inside (x, y, w, h), picking the largest integer
// scale that still fits with a small margin.
static void
sc_mini_draw_button_label(SDL_Renderer *r, int x, int y, int w, int h,
                          const char *text) {
    if (h < 9 || w < 6 || !text || !*text) {
        return;
    }
    int unit_w = sc_mini_text_width(text, 1);
    if (unit_w <= 0) {
        return;
    }
    int scale_by_h = (h - 2) / 7;
    int scale_by_w = (w - 2) / unit_w;
    int scale = scale_by_h < scale_by_w ? scale_by_h : scale_by_w;
    if (scale < 1) {
        scale = 1;
    }
    int tw = sc_mini_text_width(text, scale);
    int th = 7 * scale;
    int tx = x + (w - tw) / 2;
    int ty = y + (h - th) / 2;
    sc_mini_draw_text(r, tx, ty, scale, text);
}

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
                  enum sc_orientation orientation,
                  const SDL_Rect *toolbar_bg,
                  const SDL_Rect *toolbar_button,
                  const SDL_Rect *toolbar_logs_button,
                  const SDL_Rect *toolbar_save_logs_button,
                  const SDL_Rect *toolbar_save_all_logs_button,
                  const SDL_Rect *toolbar_home_button) {
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

    // Draw toolbar (dark strip with labelled buttons)
    if (toolbar_bg) {
        SDL_SetRenderDrawBlendMode(display->renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(display->renderer, 40, 40, 40, 255);
        SDL_RenderFillRect(display->renderer, toolbar_bg);

        // Each button reserves the bottom ~38% of its rect for a text label.
#define SC_BTN_LABEL_NUM 2
#define SC_BTN_LABEL_DEN 5
#define SC_BTN_GAP_NUM   1
#define SC_BTN_GAP_DEN   20

        // Helper: split a button rect into (icon_area, label_area).
        #define SPLIT_BUTTON(BTN, ICON, LBL) \
            int _lh = (BTN)->h * SC_BTN_LABEL_NUM / SC_BTN_LABEL_DEN; \
            int _gap = (BTN)->h * SC_BTN_GAP_NUM / SC_BTN_GAP_DEN; \
            SDL_Rect ICON = { \
                .x = (BTN)->x, .y = (BTN)->y, \
                .w = (BTN)->w, .h = (BTN)->h - _lh - _gap, \
            }; \
            SDL_Rect LBL = { \
                .x = (BTN)->x, .y = (BTN)->y + (BTN)->h - _lh, \
                .w = (BTN)->w, .h = _lh, \
            }

        if (toolbar_button) {
            // Screenshot button: lens icon + "CAM" label
            SDL_SetRenderDrawColor(display->renderer, 80, 80, 80, 255);
            SDL_RenderFillRect(display->renderer, toolbar_button);

            SPLIT_BUTTON(toolbar_button, icon, label);

            int lens_size = icon.w * 2 / 5;
            if (lens_size < 1) {
                lens_size = 1;
            }
            SDL_Rect lens = {
                .x = icon.x + (icon.w - lens_size) / 2,
                .y = icon.y + (icon.h - lens_size) / 2,
                .w = lens_size,
                .h = lens_size,
            };
            SDL_SetRenderDrawColor(display->renderer, 220, 220, 220, 255);
            SDL_RenderFillRect(display->renderer, &lens);
            sc_mini_draw_button_label(display->renderer, label.x, label.y,
                                      label.w, label.h, "CAM");
        }

        if (toolbar_logs_button) {
            // Open-live-logs button: three bars + "LOG" label
            SDL_SetRenderDrawColor(display->renderer, 80, 80, 80, 255);
            SDL_RenderFillRect(display->renderer, toolbar_logs_button);

            SPLIT_BUTTON(toolbar_logs_button, icon, label);

            int pad = icon.w / 4;
            if (pad < 1) pad = 1;
            int bar_w = icon.w - 2 * pad;
            int bar_h = icon.h / 8;
            if (bar_h < 1) bar_h = 1;
            int gap = bar_h * 2;
            int total = 3 * bar_h + 2 * gap;
            int y0 = icon.y + (icon.h - total) / 2;
            SDL_SetRenderDrawColor(display->renderer, 220, 220, 220, 255);
            for (int i = 0; i < 3; ++i) {
                SDL_Rect bar = {
                    .x = icon.x + pad,
                    .y = y0 + i * (bar_h + gap),
                    .w = bar_w,
                    .h = bar_h,
                };
                SDL_RenderFillRect(display->renderer, &bar);
            }
            sc_mini_draw_button_label(display->renderer, label.x, label.y,
                                      label.w, label.h, "LOG");
        }

        if (toolbar_save_all_logs_button) {
            // Full-system-log dump: bars + arrow + "SYS" label
            SDL_SetRenderDrawColor(display->renderer, 80, 80, 80, 255);
            SDL_RenderFillRect(display->renderer, toolbar_save_all_logs_button);

            SPLIT_BUTTON(toolbar_save_all_logs_button, icon, label);

            int bw = icon.w;
            int bh = icon.h;
            int pad = bw / 5;
            if (pad < 1) pad = 1;
            int bar_w = bw - 2 * pad;
            int bar_h = bh / 12;
            if (bar_h < 1) bar_h = 1;
            int gap = bar_h;
            int bars_total = 3 * bar_h + 2 * gap;
            int arrow_h = bh / 2;
            int content_h = bars_total + arrow_h + bar_h;
            int y0 = icon.y + (bh - content_h) / 2;

            SDL_SetRenderDrawColor(display->renderer, 220, 220, 220, 255);
            for (int i = 0; i < 3; ++i) {
                SDL_Rect bar = {
                    .x = icon.x + pad,
                    .y = y0 + i * (bar_h + gap),
                    .w = bar_w,
                    .h = bar_h,
                };
                SDL_RenderFillRect(display->renderer, &bar);
            }
            int arrow_y = y0 + bars_total + bar_h;
            int row_h = arrow_h / 3;
            if (row_h < 1) row_h = 1;
            int insets[3] = {bw / 8, bw / 4, bw * 3 / 8};
            for (int i = 0; i < 3; ++i) {
                int inset = insets[i];
                int w = bw - 2 * inset;
                if (w < 1) w = 1;
                SDL_Rect row = {
                    .x = icon.x + inset,
                    .y = arrow_y + i * row_h,
                    .w = w,
                    .h = row_h,
                };
                SDL_RenderFillRect(display->renderer, &row);
            }
            sc_mini_draw_button_label(display->renderer, label.x, label.y,
                                      label.w, label.h, "SYS");
        }

        if (toolbar_save_logs_button) {
            // App-filtered log dump: down-arrow + "APP" label
            SDL_SetRenderDrawColor(display->renderer, 80, 80, 80, 255);
            SDL_RenderFillRect(display->renderer, toolbar_save_logs_button);

            SPLIT_BUTTON(toolbar_save_logs_button, icon, label);

            int bw = icon.w;
            int bh = icon.h;
            int stem_w = bw / 3;
            if (stem_w < 1) stem_w = 1;
            int stem_h = bh * 2 / 5;
            if (stem_h < 1) stem_h = 1;
            int pad_top = bh / 8;
            int pad_bot = bh / 8;
            int tri_space = bh - pad_top - stem_h - pad_bot;
            int row_h = tri_space / 3;
            if (row_h < 1) row_h = 1;

            SDL_SetRenderDrawColor(display->renderer, 220, 220, 220, 255);

            SDL_Rect stem = {
                .x = icon.x + (bw - stem_w) / 2,
                .y = icon.y + pad_top,
                .w = stem_w,
                .h = stem_h,
            };
            SDL_RenderFillRect(display->renderer, &stem);

            int tri_y = stem.y + stem_h;
            int insets[3] = {bw / 8, bw / 4, bw * 3 / 8};
            for (int i = 0; i < 3; ++i) {
                int inset = insets[i];
                int w = bw - 2 * inset;
                if (w < 1) w = 1;
                SDL_Rect row = {
                    .x = icon.x + inset,
                    .y = tri_y + i * row_h,
                    .w = w,
                    .h = row_h,
                };
                SDL_RenderFillRect(display->renderer, &row);
            }
            sc_mini_draw_button_label(display->renderer, label.x, label.y,
                                      label.w, label.h, "APP");
        }

        if (toolbar_home_button) {
            // Home button: house icon + "HOME" label
            SDL_SetRenderDrawColor(display->renderer, 80, 80, 80, 255);
            SDL_RenderFillRect(display->renderer, toolbar_home_button);

            SPLIT_BUTTON(toolbar_home_button, icon, label);

            int pad_x = icon.w / 8;
            int pad_y = icon.h / 8;
            int hx = icon.x + pad_x;
            int hy = icon.y + pad_y;
            int hw = icon.w - 2 * pad_x;
            int hh = icon.h - 2 * pad_y;

            int roof_h = hh * 2 / 5;
            int body_h = hh - roof_h;

            SDL_SetRenderDrawColor(display->renderer, 220, 220, 220, 255);

            // Roof: 4 stacked rows widening towards the bottom
            int row_h = roof_h / 4;
            if (row_h < 1) row_h = 1;
            int insets[4] = {hw * 3 / 8, hw / 4, hw / 8, 0};
            for (int i = 0; i < 4; ++i) {
                int inset = insets[i];
                int w = hw - 2 * inset;
                if (w < 1) w = 1;
                SDL_Rect row = {
                    .x = hx + inset,
                    .y = hy + i * row_h,
                    .w = w,
                    .h = row_h,
                };
                SDL_RenderFillRect(display->renderer, &row);
            }

            // Body: filled rect under the roof
            SDL_Rect body = {
                .x = hx,
                .y = hy + roof_h,
                .w = hw,
                .h = body_h,
            };
            SDL_RenderFillRect(display->renderer, &body);

            // Door cutout (button bg color)
            int door_w = hw / 4;
            int door_h = body_h * 2 / 3;
            if (door_w >= 1 && door_h >= 1) {
                SDL_Rect door = {
                    .x = hx + (hw - door_w) / 2,
                    .y = hy + roof_h + body_h - door_h,
                    .w = door_w,
                    .h = door_h,
                };
                SDL_SetRenderDrawColor(display->renderer, 80, 80, 80, 255);
                SDL_RenderFillRect(display->renderer, &door);
            }

            sc_mini_draw_button_label(display->renderer, label.x, label.y,
                                      label.w, label.h, "HOME");
        }

#undef SPLIT_BUTTON
    }

    // Draw screenshot flash overlay if active
    if (display->flash_active) {
        sc_tick elapsed = sc_tick_now() - display->flash_start;
        sc_tick duration = SC_TICK_FROM_MS(500);
        if (elapsed < duration) {
            // Full white for first 100ms, then fade out
            uint8_t alpha;
            sc_tick fade_start = SC_TICK_FROM_MS(100);
            if (elapsed < fade_start) {
                alpha = 255;
            } else {
                alpha = (uint8_t) (255 * (duration - elapsed)
                                       / (duration - fade_start));
            }
            SDL_SetRenderDrawBlendMode(display->renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(display->renderer, 255, 255, 255, alpha);
            SDL_RenderFillRect(display->renderer, NULL);
            LOGD("Screenshot flash: alpha=%d, elapsed=%" PRItick "ms",
                 alpha, SC_TICK_TO_MS(elapsed));
        } else {
            display->flash_active = false;
            LOGD("Screenshot flash finished");
        }
    }

    SDL_RenderPresent(display->renderer);
    return SC_DISPLAY_RESULT_OK;
}

void
sc_display_flash(struct sc_display *display) {
    display->flash_start = sc_tick_now();
    display->flash_active = true;
    LOGD("Screenshot flash triggered");
}
