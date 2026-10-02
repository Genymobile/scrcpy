#ifndef SC_WINDOW_UI_MENU_H
#define SC_WINDOW_UI_MENU_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

#include "window_ui.h"

struct sc_window_ui_menu {
    void *data;
};

void
sc_window_ui_menu_init(struct sc_window_ui_menu *menu, SDL_Window *window);

void
sc_window_ui_menu_configure(struct sc_window_ui_menu *menu, bool visible,
                            bool shortcut_enabled,
                            sc_window_ui_toolbar_toggle_cb on_toggle,
                            void *userdata);

void
sc_window_ui_menu_set_toolbar_visible(struct sc_window_ui_menu *menu,
                                      bool visible);

// Return true if the event belongs to the menu and was consumed.
bool
sc_window_ui_menu_handle_event(struct sc_window_ui_menu *menu,
                               const SDL_Event *event);

void
sc_window_ui_menu_destroy(struct sc_window_ui_menu *menu);

#endif
