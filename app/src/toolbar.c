#include "toolbar.h"

#include <stddef.h>
#include <stdint.h>

// -------- Mini 5x7 bitmap font for toolbar button labels --------
//
// Each glyph occupies 5 columns x 7 rows. Rows are encoded low-5-bits,
// bit 4 = leftmost pixel.

struct sc_mini_glyph {
    char c;
    uint8_t rows[7];
};

static const struct sc_mini_glyph SC_MINI_FONT[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
};

// Integer pixel rectangle, drawn with SDL3's float API
struct irect {
    int x, y, w, h;
};

static inline void
fill(SDL_Renderer *r, struct irect rect) {
    SDL_FRect f = {rect.x, rect.y, rect.w, rect.h};
    SDL_RenderFillRect(r, &f);
}

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
                        struct irect px = {
                            .x = cx + col * scale,
                            .y = y + row * scale,
                            .w = scale,
                            .h = scale,
                        };
                        fill(r, px);
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
sc_mini_draw_button_label(SDL_Renderer *r, struct irect area,
                          const char *text) {
    if (area.h < 9 || area.w < 6 || !text || !*text) {
        return;
    }
    int unit_w = sc_mini_text_width(text, 1);
    if (unit_w <= 0) {
        return;
    }
    int scale_by_h = (area.h - 2) / 7;
    int scale_by_w = (area.w - 2) / unit_w;
    int scale = scale_by_h < scale_by_w ? scale_by_h : scale_by_w;
    if (scale < 1) {
        scale = 1;
    }
    int tw = sc_mini_text_width(text, scale);
    int th = 7 * scale;
    int tx = area.x + (area.w - tw) / 2;
    int ty = area.y + (area.h - th) / 2;
    sc_mini_draw_text(r, tx, ty, scale, text);
}

// Each button reserves the bottom 2/5 of its rect for a text label, with a
// 1/20 gap above it
static void
split_button(struct irect btn, struct irect *icon, struct irect *label) {
    int lh = btn.h * 2 / 5;
    int gap = btn.h / 20;
    *icon = (struct irect) {btn.x, btn.y, btn.w, btn.h - lh - gap};
    *label = (struct irect) {btn.x, btn.y + btn.h - lh, btn.w, lh};
}

#define BTN_BG 80, 80, 80, 255
#define BTN_FG 220, 220, 220, 255

// Fill the button background, then split it into icon and label areas
static void
begin_button(SDL_Renderer *r, struct irect btn, struct irect *icon,
             struct irect *label) {
    SDL_SetRenderDrawColor(r, BTN_BG);
    fill(r, btn);
    split_button(btn, icon, label);
    SDL_SetRenderDrawColor(r, BTN_FG);
}

static void
draw_screenshot(SDL_Renderer *r, struct irect btn) {
    // Lens icon + "CAM" label
    struct irect icon, label;
    begin_button(r, btn, &icon, &label);

    int lens_size = icon.w * 2 / 5;
    if (lens_size < 1) {
        lens_size = 1;
    }
    struct irect lens = {
        .x = icon.x + (icon.w - lens_size) / 2,
        .y = icon.y + (icon.h - lens_size) / 2,
        .w = lens_size,
        .h = lens_size,
    };
    fill(r, lens);
    sc_mini_draw_button_label(r, label, "CAM");
}

static void
draw_logs(SDL_Renderer *r, struct irect btn) {
    // Open-live-logs: three bars + "LOG" label
    struct irect icon, label;
    begin_button(r, btn, &icon, &label);

    int pad = icon.w / 4;
    if (pad < 1) pad = 1;
    int bar_w = icon.w - 2 * pad;
    int bar_h = icon.h / 8;
    if (bar_h < 1) bar_h = 1;
    int gap = bar_h * 2;
    int total = 3 * bar_h + 2 * gap;
    int y0 = icon.y + (icon.h - total) / 2;
    for (int i = 0; i < 3; ++i) {
        struct irect bar = {icon.x + pad, y0 + i * (bar_h + gap), bar_w, bar_h};
        fill(r, bar);
    }
    sc_mini_draw_button_label(r, label, "LOG");
}

static void
draw_save_all_logs(SDL_Renderer *r, struct irect btn) {
    // Full-system-log dump: bars + arrow + "SYS" label
    struct irect icon, label;
    begin_button(r, btn, &icon, &label);

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

    for (int i = 0; i < 3; ++i) {
        struct irect bar = {icon.x + pad, y0 + i * (bar_h + gap), bar_w, bar_h};
        fill(r, bar);
    }
    int arrow_y = y0 + bars_total + bar_h;
    int row_h = arrow_h / 3;
    if (row_h < 1) row_h = 1;
    int insets[3] = {bw / 8, bw / 4, bw * 3 / 8};
    for (int i = 0; i < 3; ++i) {
        int w = bw - 2 * insets[i];
        if (w < 1) w = 1;
        struct irect row = {icon.x + insets[i], arrow_y + i * row_h, w, row_h};
        fill(r, row);
    }
    sc_mini_draw_button_label(r, label, "SYS");
}

static void
draw_save_logs(SDL_Renderer *r, struct irect btn) {
    // App-filtered log dump: down-arrow + "APP" label
    struct irect icon, label;
    begin_button(r, btn, &icon, &label);

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

    struct irect stem = {
        .x = icon.x + (bw - stem_w) / 2,
        .y = icon.y + pad_top,
        .w = stem_w,
        .h = stem_h,
    };
    fill(r, stem);

    int tri_y = stem.y + stem_h;
    int insets[3] = {bw / 8, bw / 4, bw * 3 / 8};
    for (int i = 0; i < 3; ++i) {
        int w = bw - 2 * insets[i];
        if (w < 1) w = 1;
        struct irect row = {icon.x + insets[i], tri_y + i * row_h, w, row_h};
        fill(r, row);
    }
    sc_mini_draw_button_label(r, label, "APP");
}

static void
draw_home(SDL_Renderer *r, struct irect btn) {
    // House icon + "HOME" label
    struct irect icon, label;
    begin_button(r, btn, &icon, &label);

    int pad_x = icon.w / 8;
    int pad_y = icon.h / 8;
    int hx = icon.x + pad_x;
    int hy = icon.y + pad_y;
    int hw = icon.w - 2 * pad_x;
    int hh = icon.h - 2 * pad_y;

    int roof_h = hh * 2 / 5;
    int body_h = hh - roof_h;

    // Roof: 4 stacked rows widening towards the bottom
    int row_h = roof_h / 4;
    if (row_h < 1) row_h = 1;
    int insets[4] = {hw * 3 / 8, hw / 4, hw / 8, 0};
    for (int i = 0; i < 4; ++i) {
        int w = hw - 2 * insets[i];
        if (w < 1) w = 1;
        struct irect row = {hx + insets[i], hy + i * row_h, w, row_h};
        fill(r, row);
    }

    // Body: filled rect under the roof
    struct irect body = {hx, hy + roof_h, hw, body_h};
    fill(r, body);

    // Door cutout (button bg color)
    int door_w = hw / 4;
    int door_h = body_h * 2 / 3;
    if (door_w >= 1 && door_h >= 1) {
        struct irect door = {
            .x = hx + (hw - door_w) / 2,
            .y = hy + roof_h + body_h - door_h,
            .w = door_w,
            .h = door_h,
        };
        SDL_SetRenderDrawColor(r, BTN_BG);
        fill(r, door);
    }

    sc_mini_draw_button_label(r, label, "HOME");
}

static void
draw_expand(SDL_Renderer *r, struct irect btn) {
    // Four corner brackets + "FIT" label
    struct irect icon, label;
    begin_button(r, btn, &icon, &label);

    int pad = icon.w / 6;
    if (pad < 1) pad = 1;
    int t = icon.h / 10;
    if (t < 1) t = 1;
    int arm = icon.w / 3;
    if (arm < 1) arm = 1;
    int x0 = icon.x + pad;
    int y0 = icon.y + pad;
    int x1 = icon.x + icon.w - pad;
    int y1 = icon.y + icon.h - pad;

    struct irect brackets[8] = {
        // top-left
        {x0, y0, arm, t}, {x0, y0, t, arm},
        // top-right
        {x1 - arm, y0, arm, t}, {x1 - t, y0, t, arm},
        // bottom-left
        {x0, y1 - t, arm, t}, {x0, y1 - arm, t, arm},
        // bottom-right
        {x1 - arm, y1 - t, arm, t}, {x1 - t, y1 - arm, t, arm},
    };
    for (int i = 0; i < 8; ++i) {
        fill(r, brackets[i]);
    }
    sc_mini_draw_button_label(r, label, "FIT");
}

void
sc_toolbar_layout(struct sc_toolbar *tb, float window_width) {
    float h = SC_TOOLBAR_HEIGHT;

    // The toolbar spans the full width at the top
    tb->bg = (SDL_FRect) {0, 0, window_width, h};

    // Square buttons, right-aligned, from right to left:
    // screenshot, logs, save-logs, save-all-logs, home, expand
    float margin = h / 4;
    float size = h - 2 * margin;
    SDL_FRect *order[] = {
        &tb->screenshot, &tb->logs, &tb->save_logs, &tb->save_all_logs,
        &tb->home, &tb->expand,
    };
    float x = window_width - margin - size;
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        *order[i] = (SDL_FRect) {x, margin, size, size};
        x -= margin + size;
    }
}

// Convert a window-coordinates rect to an integer pixel rect
static struct irect
to_pixels(const SDL_FRect *rect, float scale) {
    int x0 = (int) (rect->x * scale);
    int y0 = (int) (rect->y * scale);
    int x1 = (int) ((rect->x + rect->w) * scale);
    int y1 = (int) ((rect->y + rect->h) * scale);
    return (struct irect) {x0, y0, x1 - x0, y1 - y0};
}

void
sc_toolbar_render(const struct sc_toolbar *tb, SDL_Renderer *renderer,
                  float scale) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    fill(renderer, to_pixels(&tb->bg, scale));

    draw_screenshot(renderer, to_pixels(&tb->screenshot, scale));
    draw_logs(renderer, to_pixels(&tb->logs, scale));
    draw_save_all_logs(renderer, to_pixels(&tb->save_all_logs, scale));
    draw_save_logs(renderer, to_pixels(&tb->save_logs, scale));
    draw_home(renderer, to_pixels(&tb->home, scale));
    draw_expand(renderer, to_pixels(&tb->expand, scale));
}

bool
sc_toolbar_contains(const struct sc_toolbar *tb, float x, float y) {
    (void) x;
    return y < tb->bg.y + tb->bg.h;
}

static inline bool
in_rect(const SDL_FRect *r, float x, float y) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

enum sc_toolbar_button
sc_toolbar_hit(const struct sc_toolbar *tb, float x, float y) {
    if (in_rect(&tb->screenshot, x, y)) {
        return SC_TOOLBAR_BUTTON_SCREENSHOT;
    }
    if (in_rect(&tb->logs, x, y)) {
        return SC_TOOLBAR_BUTTON_LOGS;
    }
    if (in_rect(&tb->save_logs, x, y)) {
        return SC_TOOLBAR_BUTTON_SAVE_LOGS;
    }
    if (in_rect(&tb->save_all_logs, x, y)) {
        return SC_TOOLBAR_BUTTON_SAVE_ALL_LOGS;
    }
    if (in_rect(&tb->home, x, y)) {
        return SC_TOOLBAR_BUTTON_HOME;
    }
    if (in_rect(&tb->expand, x, y)) {
        return SC_TOOLBAR_BUTTON_EXPAND;
    }
    return SC_TOOLBAR_BUTTON_NONE;
}
