#include "emulator_ui.h"

#include <string.h>

#include "util/log.h"

static bool
sc_emulator_ui_is_visible(const struct sc_emulator_ui *ui) {
    if (!ui->toolbar.enabled || !ui->toolbar.window
            || !ui->toolbar.user_visible || !ui->parent_visible
            || ui->parent_fullscreen) {
        return false;
    }

    uint64_t flags = SDL_GetWindowFlags(ui->parent);
    return !(flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
}

static void
sc_emulator_ui_render(struct sc_emulator_ui *ui) {
    sc_toolbar_render(&ui->toolbar, &ui->state);
}

static void
sc_emulator_ui_sync(struct sc_emulator_ui *ui) {
    if (!ui->toolbar.enabled || !ui->toolbar.window) {
        return;
    }

    if (sc_emulator_ui_is_visible(ui)) {
        sc_toolbar_update_position(&ui->toolbar, ui->parent);
        sc_emulator_ui_render(ui);
        sc_toolbar_show(&ui->toolbar);
    } else {
        sc_toolbar_hide(&ui->toolbar);
    }
}

static void
sc_emulator_ui_toggle_toolbar(void *userdata) {
    struct sc_emulator_ui *ui = userdata;
    ui->toolbar.user_visible = !ui->toolbar.user_visible;
    ui->toolbar.hovered = SC_TOOLBAR_ACTION_NONE;
    ui->toolbar.pressed = SC_TOOLBAR_ACTION_NONE;
    LOGD("%s floating toolbar from application menu",
         ui->toolbar.user_visible ? "Showing" : "Hiding");
    sc_window_ui_set_toolbar_menu_visible(&ui->window_ui,
                                          ui->toolbar.user_visible);
    sc_emulator_ui_sync(ui);
}

void
sc_emulator_ui_init(struct sc_emulator_ui *ui, SDL_Window *parent,
                    bool enabled, bool always_on_top,
                    const char *title_primary, const char *title_secondary,
                    const struct sc_emulator_ui_callbacks *callbacks,
                    void *callbacks_userdata) {
    memset(ui, 0, sizeof(*ui));
    ui->parent = parent;
    ui->parent_id = SDL_GetWindowID(parent);
    ui->callbacks = callbacks;
    ui->callbacks_userdata = callbacks_userdata;
    sc_toolbar_init(&ui->toolbar, enabled);
    sc_window_ui_init(&ui->window_ui, parent);

    if (!enabled) {
        return;
    }

    sc_window_ui_set_device_title(&ui->window_ui, title_primary,
                                  title_secondary);
    if (!sc_toolbar_create(&ui->toolbar, parent, always_on_top)) {
        LOGW("Floating toolbar unavailable, continuing without it");
        ui->toolbar.enabled = false;
        return;
    }

    sc_window_ui_configure_toolbar_menu(
        &ui->window_ui, ui->toolbar.user_visible,
        sc_emulator_ui_toggle_toolbar, ui);
}

void
sc_emulator_ui_destroy(struct sc_emulator_ui *ui) {
    sc_toolbar_destroy(&ui->toolbar);
    sc_window_ui_destroy(&ui->window_ui);
}

void
sc_emulator_ui_set_parent_visible(struct sc_emulator_ui *ui, bool visible) {
    if (ui->parent_visible == visible) {
        return;
    }
    ui->parent_visible = visible;
    sc_emulator_ui_sync(ui);
}

void
sc_emulator_ui_set_parent_fullscreen(struct sc_emulator_ui *ui,
                                     bool fullscreen) {
    if (ui->parent_fullscreen == fullscreen) {
        return;
    }
    ui->parent_fullscreen = fullscreen;
    ui->toolbar.hovered = SC_TOOLBAR_ACTION_NONE;
    ui->toolbar.pressed = SC_TOOLBAR_ACTION_NONE;
    sc_emulator_ui_sync(ui);
}

void
sc_emulator_ui_set_state(struct sc_emulator_ui *ui,
                         const struct sc_toolbar_view_state *state) {
    if (ui->state.navigation_enabled == state->navigation_enabled
            && ui->state.screenshot_enabled == state->screenshot_enabled
            && ui->state.record_enabled == state->record_enabled
            && ui->state.recording == state->recording) {
        return;
    }
    ui->state = *state;
    if (sc_emulator_ui_is_visible(ui)) {
        sc_emulator_ui_render(ui);
    }
}

void
sc_emulator_ui_show_toast(struct sc_emulator_ui *ui, const char *message) {
    sc_window_ui_show_toast(&ui->window_ui, message);
}

static void
sc_emulator_ui_update_hover(struct sc_emulator_ui *ui,
                            enum sc_toolbar_action action) {
    if (ui->toolbar.hovered == action) {
        return;
    }
    ui->toolbar.hovered = action;
    sc_emulator_ui_render(ui);
}

static bool
sc_emulator_ui_handle_toolbar_event(struct sc_emulator_ui *ui,
                                    const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION: {
            if (!sc_toolbar_is_window(&ui->toolbar,
                                      event->motion.windowID)) {
                return false;
            }
            if (sc_emulator_ui_is_visible(ui)) {
                enum sc_toolbar_action action =
                    sc_toolbar_hit_test(&ui->toolbar, event->motion.x,
                                        event->motion.y);
                sc_emulator_ui_update_hover(ui, action);
            }
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!sc_toolbar_is_window(&ui->toolbar,
                                      event->button.windowID)) {
                return false;
            }
            if (sc_emulator_ui_is_visible(ui)
                    && event->button.button == SDL_BUTTON_LEFT) {
                ui->toolbar.pressed =
                    sc_toolbar_hit_test(&ui->toolbar, event->button.x,
                                        event->button.y);
                sc_emulator_ui_render(ui);
            }
            return true;
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (!sc_toolbar_is_window(&ui->toolbar,
                                      event->button.windowID)) {
                return false;
            }
            enum sc_toolbar_action pressed = ui->toolbar.pressed;
            ui->toolbar.pressed = SC_TOOLBAR_ACTION_NONE;
            if (!sc_emulator_ui_is_visible(ui)
                    || event->button.button != SDL_BUTTON_LEFT) {
                return true;
            }
            enum sc_toolbar_action released =
                sc_toolbar_hit_test(&ui->toolbar, event->button.x,
                                    event->button.y);
            if (pressed != SC_TOOLBAR_ACTION_NONE && pressed == released
                    && ui->callbacks && ui->callbacks->on_action) {
                ui->callbacks->on_action(ui, pressed,
                                         ui->callbacks_userdata);
            }
            sc_emulator_ui_render(ui);
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            return sc_toolbar_is_window(&ui->toolbar,
                                        event->wheel.windowID);
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            if (!sc_toolbar_is_window(&ui->toolbar,
                                      event->window.windowID)) {
                return false;
            }
            ui->toolbar.pressed = SC_TOOLBAR_ACTION_NONE;
            sc_emulator_ui_update_hover(ui, SC_TOOLBAR_ACTION_NONE);
            return true;
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (!sc_toolbar_is_window(&ui->toolbar,
                                      event->window.windowID)) {
                return false;
            }
            sc_emulator_ui_render(ui);
            return true;
        default:
            return false;
    }
}

bool
sc_emulator_ui_handle_event(struct sc_emulator_ui *ui,
                            const SDL_Event *event) {
    if (sc_emulator_ui_handle_toolbar_event(ui, event)) {
        return true;
    }

    switch (event->type) {
        case SDL_EVENT_WINDOW_MOVED:
        case SDL_EVENT_WINDOW_RESIZED:
            if (event->window.windowID == ui->parent_id) {
                sc_toolbar_update_position(&ui->toolbar, ui->parent);
            }
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_SHOWN:
            if (event->window.windowID == ui->parent_id) {
                ui->parent_visible = true;
                sc_emulator_ui_sync(ui);
            }
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_HIDDEN:
            if (event->window.windowID == ui->parent_id) {
                ui->parent_visible = false;
                sc_emulator_ui_sync(ui);
            }
            break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            if (event->window.windowID == ui->parent_id) {
                sc_emulator_ui_set_parent_fullscreen(ui, true);
            }
            break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            if (event->window.windowID == ui->parent_id) {
                sc_emulator_ui_set_parent_fullscreen(ui, false);
            }
            break;
        default:
            break;
    }
    return false;
}
