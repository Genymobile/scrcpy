#ifndef SC_OVERLAY_UI_H
#define SC_OVERLAY_UI_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "util/tick.h"

struct sc_overlay {
    // Template switch popup
    bool switch_visible;
    char **names;
    int count;
    int hovered;          // index under cursor, -1 if none
    SDL_FRect popup_rect; // overall popup bounds (window coords)

    // Design mode
    bool design_mode;
    char design_hint[256];

    // Toggle indicator (brief flash)
    bool show_toggle;
    bool mapping_enabled;
    sc_tick toggle_until;

    // status line (bottom of window)
    char status[256];
    sc_tick status_until;
};

void
sc_overlay_init(struct sc_overlay *ov);

void
sc_overlay_destroy(struct sc_overlay *ov);

// Show the template switch popup with the given names.
// Takes ownership of the names array (will free on hide).
void
sc_overlay_show_switch(struct sc_overlay *ov, char **names, int count);

void
sc_overlay_hide_switch(struct sc_overlay *ov);

// Update hover state from mouse position. Returns the hovered index or -1.
int
sc_overlay_update_switch_mouse(struct sc_overlay *ov, float mx, float my);

// Set design mode and hint text.
void
sc_overlay_set_design(struct sc_overlay *ov, bool active, const char *hint);

// Flash the mapping-enabled/disabled indicator.
void
sc_overlay_show_toggle(struct sc_overlay *ov, bool enabled);

// Set a transient status message.
void
sc_overlay_set_status(struct sc_overlay *ov, const char *msg);

// Render all overlay elements on top of the frame.
void
sc_overlay_render(struct sc_overlay *ov, SDL_Renderer *renderer,
                  int win_w, int win_h);

#endif
