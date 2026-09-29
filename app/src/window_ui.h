#ifndef SC_WINDOW_UI_H
#define SC_WINDOW_UI_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef void (*sc_window_ui_toolbar_toggle_cb)(void *userdata);

struct sc_window_ui {
    SDL_Window *window;
    void *data;
};

void
sc_window_ui_init(struct sc_window_ui *ui, SDL_Window *window);

void
sc_window_ui_set_device_title(struct sc_window_ui *ui, const char *primary,
                              const char *secondary);

void
sc_window_ui_show_toast(struct sc_window_ui *ui, const char *message);

void
sc_window_ui_configure_toolbar_menu(struct sc_window_ui *ui, bool visible,
                                    sc_window_ui_toolbar_toggle_cb on_toggle,
                                    void *userdata);

void
sc_window_ui_set_toolbar_menu_visible(struct sc_window_ui *ui, bool visible);

void
sc_window_ui_destroy(struct sc_window_ui *ui);

#endif
