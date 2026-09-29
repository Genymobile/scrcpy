#ifndef SC_TOOLBAR_H
#define SC_TOOLBAR_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

#define SC_TOOLBAR_PANEL_WIDTH 48
#define SC_TOOLBAR_BUTTON_HEIGHT 48
#define SC_TOOLBAR_ICON_SIZE 16
#define SC_TOOLBAR_SEPARATOR_HEIGHT 8
#define SC_TOOLBAR_PANEL_HEIGHT \
    (SC_TOOLBAR_BUTTON_HEIGHT * 5 + SC_TOOLBAR_SEPARATOR_HEIGHT)
#define SC_TOOLBAR_SHADOW_MARGIN 5
#define SC_TOOLBAR_WINDOW_WIDTH \
    (SC_TOOLBAR_PANEL_WIDTH + 2 * SC_TOOLBAR_SHADOW_MARGIN)
#define SC_TOOLBAR_WINDOW_HEIGHT \
    (SC_TOOLBAR_PANEL_HEIGHT + 2 * SC_TOOLBAR_SHADOW_MARGIN)
#define SC_TOOLBAR_GAP 12
#define SC_TOOLBAR_TOP_OFFSET 2

enum sc_toolbar_action {
    SC_TOOLBAR_ACTION_NONE,
    SC_TOOLBAR_ACTION_HOME,
    SC_TOOLBAR_ACTION_BACK,
    SC_TOOLBAR_ACTION_RECENTS,
    SC_TOOLBAR_ACTION_SCREENSHOT,
    SC_TOOLBAR_ACTION_RECORD,
    SC_TOOLBAR_ACTION_COUNT,
};

struct sc_toolbar {
    bool enabled;
    bool user_visible;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *icons[SC_TOOLBAR_ACTION_COUNT];
    int icon_asset_size;
    SDL_Texture *render_cache;
    int render_cache_width;
    int render_cache_height;
    bool render_cache_navigation_enabled;
    bool render_cache_screenshot_enabled;
    bool render_cache_record_enabled;
    bool render_cache_recording;
    enum sc_toolbar_action render_cache_hovered;
    enum sc_toolbar_action render_cache_pressed;
    SDL_FRect panel;
    enum sc_toolbar_action hovered;
    enum sc_toolbar_action pressed;
    SDL_FRect buttons[SC_TOOLBAR_ACTION_COUNT];
};

struct sc_toolbar_view_state {
    bool navigation_enabled;
    bool screenshot_enabled;
    bool record_enabled;
    bool recording;
};

void
sc_toolbar_init(struct sc_toolbar *toolbar, bool enabled);

bool
sc_toolbar_create(struct sc_toolbar *toolbar, SDL_Window *parent,
                  bool always_on_top);

void
sc_toolbar_destroy(struct sc_toolbar *toolbar);

void
sc_toolbar_layout(struct sc_toolbar *toolbar);

enum sc_toolbar_action
sc_toolbar_hit_test(const struct sc_toolbar *toolbar, float x, float y);

bool
sc_toolbar_contains(const struct sc_toolbar *toolbar, float x, float y);

bool
sc_toolbar_is_window(const struct sc_toolbar *toolbar, SDL_WindowID window_id);

void
sc_toolbar_update_position(struct sc_toolbar *toolbar, SDL_Window *parent);

void
sc_toolbar_show(struct sc_toolbar *toolbar);

void
sc_toolbar_hide(struct sc_toolbar *toolbar);

void
sc_toolbar_render(struct sc_toolbar *toolbar,
                  const struct sc_toolbar_view_state *state);

#endif
