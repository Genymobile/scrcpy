#include "overlay_ui.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "util/log.h"
#include "util/tick.h"

#define POPUP_WIDTH 320
#define ROW_HEIGHT 40
#define POPUP_PADDING 12

void
sc_overlay_init(struct sc_overlay *ov) {
    memset(ov, 0, sizeof(*ov));
    ov->hovered = -1;
}

void
sc_overlay_destroy(struct sc_overlay *ov) {
    sc_overlay_hide_switch(ov);
}

void
sc_overlay_show_switch(struct sc_overlay *ov, char **names, int count) {
    sc_overlay_hide_switch(ov);
    ov->names = names;
    ov->count = count;
    ov->switch_visible = true;
    ov->hovered = -1;
}

void
sc_overlay_hide_switch(struct sc_overlay *ov) {
    if (ov->names) {
        for (int i = 0; i < ov->count; i++) {
            free(ov->names[i]);
        }
        free(ov->names);
        ov->names = NULL;
    }
    ov->count = 0;
    ov->switch_visible = false;
    ov->hovered = -1;
}

int
sc_overlay_update_switch_mouse(struct sc_overlay *ov, float mx, float my) {
    if (!ov->switch_visible) return -1;
    const SDL_FRect *r = &ov->popup_rect;
    if (mx < r->x || mx >= r->x + r->w || my < r->y || my >= r->y + r->h) {
        ov->hovered = -1;
        return -1;
    }
    float rel_y = my - r->y - POPUP_PADDING;
    int idx = (int)(rel_y / ROW_HEIGHT);
    if (idx < 0 || idx >= ov->count) {
        ov->hovered = -1;
        return -1;
    }
    ov->hovered = idx;
    return idx;
}

void
sc_overlay_set_design(struct sc_overlay *ov, bool active, const char *hint) {
    ov->design_mode = active;
    if (hint) {
        strncpy(ov->design_hint, hint, sizeof(ov->design_hint) - 1);
        ov->design_hint[sizeof(ov->design_hint) - 1] = '\0';
    } else {
        ov->design_hint[0] = '\0';
    }
}

void
sc_overlay_show_toggle(struct sc_overlay *ov, bool enabled) {
    ov->show_toggle = true;
    ov->mapping_enabled = enabled;
    ov->toggle_until = sc_tick_now() + SC_TICK_FROM_MS(1500);
}

void
sc_overlay_set_status(struct sc_overlay *ov, const char *msg) {
    if (msg) {
        strncpy(ov->status, msg, sizeof(ov->status) - 1);
        ov->status[sizeof(ov->status) - 1] = '\0';
        ov->status_until = sc_tick_now() + SC_TICK_FROM_MS(3000);
    } else {
        ov->status[0] = '\0';
    }
}

// Draw a filled rounded-ish rectangle (plain rect with optional border)
static void
draw_panel(SDL_Renderer *r, float x, float y, float w, float h,
           Uint8 br, Uint8 bg, Uint8 bb, Uint8 ba) {
    SDL_SetRenderDrawColor(r, br, bg, bb, ba);
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void
draw_border(SDL_Renderer *r, float x, float y, float w, float h,
            Uint8 br, Uint8 bg, Uint8 bb, Uint8 ba) {
    SDL_SetRenderDrawColor(r, br, bg, bb, ba);
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderRect(r, &rect);
}

static void
draw_text(SDL_Renderer *r, float x, float y, const char *text,
          Uint8 cr, Uint8 cg, Uint8 cb) {
    SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    SDL_RenderDebugText(r, x, y, text);
}

void
sc_overlay_render(struct sc_overlay *ov, SDL_Renderer *renderer,
                  int win_w, int win_h) {
    // ---- Template switch popup ----
    if (ov->switch_visible && ov->count > 0) {
        float popup_h = POPUP_PADDING * 2 + ov->count * ROW_HEIGHT;
        float px = (win_w - POPUP_WIDTH) / 2.0f;
        float py = (win_h - popup_h) / 2.0f;
        ov->popup_rect.x = px;
        ov->popup_rect.y = py;
        ov->popup_rect.w = POPUP_WIDTH;
        ov->popup_rect.h = popup_h;

        // background
        draw_panel(renderer, px, py, POPUP_WIDTH, popup_h, 20, 20, 20, 235);
        draw_border(renderer, px, py, POPUP_WIDTH, popup_h, 120, 120, 120, 255);

        // title
        draw_text(renderer, px + POPUP_PADDING, py + 4, "Select Template",
                  200, 200, 200);

        // rows
        for (int i = 0; i < ov->count; i++) {
            float ry = py + POPUP_PADDING + i * ROW_HEIGHT;
            bool is_hovered = (i == ov->hovered);
            if (is_hovered) {
                draw_panel(renderer, px + 4, ry, POPUP_WIDTH - 8, ROW_HEIGHT - 4,
                           60, 120, 220, 230);
            }
            const char *name = ov->names[i] ? ov->names[i] : "";
            draw_text(renderer, px + POPUP_PADDING, ry + 12, name,
                      255, 255, 255);
        }
    }

    // ---- Design mode banner ----
    if (ov->design_mode) {
        float banner_h = 36;
        draw_panel(renderer, 0, 0, (float)win_w, banner_h, 200, 100, 0, 200);
        const char *hint = ov->design_hint[0] ? ov->design_hint
                             : "Design Mode - click a position, then press a key";
        draw_text(renderer, 12, 12, hint, 255, 255, 255);
    }

    // ---- Toggle indicator ----
    if (ov->show_toggle) {
        if (sc_tick_now() > ov->toggle_until) {
            ov->show_toggle = false;
        } else {
            float iw = 220, ih = 36;
            float ix = (win_w - iw) / 2.0f;
            float iy = (win_h - ih) / 2.0f;
            Uint8 r = ov->mapping_enabled ? 40 : 180;
            Uint8 g = ov->mapping_enabled ? 160 : 40;
            Uint8 b = 60;
            draw_panel(renderer, ix, iy, iw, ih, r, g, b, 220);
            const char *txt = ov->mapping_enabled ? "Mapping: ON" : "Mapping: OFF";
            draw_text(renderer, ix + 12, iy + 12, txt, 255, 255, 255);
        }
    }

    // ---- Status line (bottom) ----
    if (ov->status[0]) {
        if (sc_tick_now() > ov->status_until) {
            ov->status[0] = '\0';
        } else {
            float sh = 24;
            draw_panel(renderer, 0, (float)(win_h - sh), (float)win_w, sh,
                       0, 0, 0, 180);
            draw_text(renderer, 12, (float)(win_h - sh) + 6, ov->status,
                      200, 220, 255);
        }
    }
}
