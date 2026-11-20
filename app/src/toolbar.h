#ifndef SC_TOOLBAR_H
#define SC_TOOLBAR_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

// Height of the toolbar strip above the content, in window coordinates
// (points, not physical pixels)
#define SC_TOOLBAR_HEIGHT 72

enum sc_toolbar_button {
    SC_TOOLBAR_BUTTON_NONE,
    SC_TOOLBAR_BUTTON_EXPAND,        // "FIT": resize the window to the display
    SC_TOOLBAR_BUTTON_HOME,          // "HOME": send Android HOME
    SC_TOOLBAR_BUTTON_SAVE_ALL_LOGS, // "SYS": dump all logcat buffers
    SC_TOOLBAR_BUTTON_SAVE_LOGS,     // "APP": dump foreground-app logcat
    SC_TOOLBAR_BUTTON_LOGS,          // "LOG": open live logcat in a terminal
    SC_TOOLBAR_BUTTON_SCREENSHOT,    // "CAM": screenshot to file + clipboard
};

// All rectangles are in window coordinates
struct sc_toolbar {
    SDL_FRect bg; // the whole strip
    SDL_FRect screenshot;
    SDL_FRect logs;
    SDL_FRect save_logs;
    SDL_FRect save_all_logs;
    SDL_FRect home;
    SDL_FRect expand;
};

// Compute the toolbar geometry for a window of the given width
void
sc_toolbar_layout(struct sc_toolbar *tb, float window_width);

// Draw the toolbar; `scale` is the window pixel density
void
sc_toolbar_render(const struct sc_toolbar *tb, SDL_Renderer *renderer,
                  float scale);

// Return true if the point (window coordinates) is inside the toolbar strip
bool
sc_toolbar_contains(const struct sc_toolbar *tb, float x, float y);

// Return the button under the point (window coordinates), if any
enum sc_toolbar_button
sc_toolbar_hit(const struct sc_toolbar *tb, float x, float y);

#endif
