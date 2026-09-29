#ifndef SC_EMULATOR_UI_H
#define SC_EMULATOR_UI_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

#include "toolbar.h"
#include "window_ui.h"

struct sc_emulator_ui;

struct sc_emulator_ui_callbacks {
    void (*on_action)(struct sc_emulator_ui *ui,
                      enum sc_toolbar_action action, void *userdata);
};

struct sc_emulator_ui {
    SDL_Window *parent;
    SDL_WindowID parent_id;
    struct sc_toolbar toolbar;
    struct sc_toolbar_view_state state;
    struct sc_window_ui window_ui;
    const struct sc_emulator_ui_callbacks *callbacks;
    void *callbacks_userdata;
    bool parent_visible;
    bool parent_fullscreen;
};

void
sc_emulator_ui_init(struct sc_emulator_ui *ui, SDL_Window *parent,
                    bool enabled, bool always_on_top, bool shortcut_enabled,
                    const char *title_primary, const char *title_secondary,
                    const struct sc_emulator_ui_callbacks *callbacks,
                    void *callbacks_userdata);

void
sc_emulator_ui_destroy(struct sc_emulator_ui *ui);

void
sc_emulator_ui_set_parent_visible(struct sc_emulator_ui *ui, bool visible);

void
sc_emulator_ui_set_parent_fullscreen(struct sc_emulator_ui *ui,
                                     bool fullscreen);

void
sc_emulator_ui_set_state(struct sc_emulator_ui *ui,
                         const struct sc_toolbar_view_state *state);

void
sc_emulator_ui_show_toast(struct sc_emulator_ui *ui, const char *message);

// Return true if the event belongs to the emulator UI and was consumed.
bool
sc_emulator_ui_handle_event(struct sc_emulator_ui *ui,
                            const SDL_Event *event);

#endif
