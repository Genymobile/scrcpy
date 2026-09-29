#include "window_ui_menu.h"

#include <math.h>
#include <stdlib.h>

#include "util/log.h"

#define SC_VIEW_MENU_WIDTH 224
#define SC_VIEW_MENU_TITLE_HEIGHT 28
#define SC_VIEW_MENU_ITEM_HEIGHT 40
#define SC_VIEW_MENU_HEIGHT \
    (SC_VIEW_MENU_TITLE_HEIGHT + SC_VIEW_MENU_ITEM_HEIGHT)
#define SC_VIEW_MENU_RADIUS 10.f

struct sc_window_ui_menu_data {
    SDL_Window *parent;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_WindowID window_id;
    bool hovered;
    bool toolbar_visible;
    bool shortcut_enabled;
    sc_window_ui_toolbar_toggle_cb toolbar_toggle_callback;
    void *toolbar_toggle_userdata;
};

static void
sc_window_ui_menu_fill_rounded_rect(SDL_Renderer *renderer, SDL_FRect rect,
                                    float radius) {
    int y_begin = (int) ceilf(rect.y);
    int y_end = (int) floorf(rect.y + rect.h);
    float top_center = rect.y + radius;
    float bottom_center = rect.y + rect.h - radius;
    for (int y = y_begin; y < y_end; ++y) {
        float py = y + .5f;
        float inset = 0.f;
        if (py < top_center) {
            float dy = top_center - py;
            inset = radius - sqrtf(radius * radius - dy * dy);
        } else if (py > bottom_center) {
            float dy = py - bottom_center;
            inset = radius - sqrtf(radius * radius - dy * dy);
        }
        SDL_RenderLine(renderer, ceilf(rect.x + inset), y + .5f,
                       floorf(rect.x + rect.w - inset), y + .5f);
    }
}

static void
sc_window_ui_menu_toggle_toolbar(struct sc_window_ui_menu_data *data) {
    if (data->toolbar_toggle_callback) {
        data->toolbar_toggle_callback(data->toolbar_toggle_userdata);
    }
}

static bool
sc_window_ui_menu_is_toggle_shortcut(const SDL_KeyboardEvent *event) {
    uint16_t modifiers = event->mod;
    return event->key == SDLK_T && !event->repeat
        && (modifiers & SDL_KMOD_CTRL)
        && (modifiers & SDL_KMOD_SHIFT)
        && !(modifiers & (SDL_KMOD_ALT | SDL_KMOD_GUI));
}

static void
sc_window_ui_menu_render(struct sc_window_ui_menu_data *data) {
    if (!data->renderer) {
        return;
    }

    float density = SDL_GetWindowPixelDensity(data->window);
    if (!density) {
        density = 1.f;
    }
    SDL_SetRenderScale(data->renderer, density, density);
    SDL_SetRenderDrawColor(data->renderer, 0, 0, 0, 0);
    SDL_RenderClear(data->renderer);

    SDL_SetRenderDrawColor(data->renderer, 28, 30, 31, 248);
    sc_window_ui_menu_fill_rounded_rect(
        data->renderer,
        (SDL_FRect) {0, 0, SC_VIEW_MENU_WIDTH, SC_VIEW_MENU_HEIGHT},
        SC_VIEW_MENU_RADIUS);

    SDL_SetRenderDrawColor(data->renderer, 166, 166, 166, 255);
    SDL_RenderDebugText(data->renderer, 12, 10, "View");

    if (data->hovered) {
        SDL_SetRenderDrawColor(data->renderer, 54, 56, 57, 255);
        sc_window_ui_menu_fill_rounded_rect(
            data->renderer,
            (SDL_FRect) {4, SC_VIEW_MENU_TITLE_HEIGHT,
                         SC_VIEW_MENU_WIDTH - 8,
                         SC_VIEW_MENU_ITEM_HEIGHT - 4},
            7.f);
    }

    SDL_SetRenderDrawColor(data->renderer, 244, 244, 242, 255);
    const char *label = data->toolbar_visible ? "Hide Toolbar"
                                              : "Show Toolbar";
    SDL_RenderDebugText(data->renderer, 12,
                        SC_VIEW_MENU_TITLE_HEIGHT + 12, label);
    if (data->shortcut_enabled) {
        SDL_SetRenderDrawColor(data->renderer, 150, 150, 150, 255);
        SDL_RenderDebugText(data->renderer, 124,
                            SC_VIEW_MENU_TITLE_HEIGHT + 12, "Ctrl+Shift+T");
    }
    SDL_RenderPresent(data->renderer);
}

static bool
sc_window_ui_menu_create(struct sc_window_ui_menu_data *data) {
    data->window = SDL_CreatePopupWindow(
        data->parent, 8, 8, SC_VIEW_MENU_WIDTH, SC_VIEW_MENU_HEIGHT,
        SDL_WINDOW_POPUP_MENU | SDL_WINDOW_HIGH_PIXEL_DENSITY
                              | SDL_WINDOW_HIDDEN
                              | SDL_WINDOW_TRANSPARENT);
    if (!data->window) {
        LOGW("Could not create View menu: %s", SDL_GetError());
        return false;
    }

    data->renderer = SDL_CreateRenderer(data->window, NULL);
    if (!data->renderer) {
        LOGW("Could not create View menu renderer: %s", SDL_GetError());
        SDL_DestroyWindow(data->window);
        data->window = NULL;
        return false;
    }
    SDL_SetRenderDrawBlendMode(data->renderer, SDL_BLENDMODE_BLEND);
    data->window_id = SDL_GetWindowID(data->window);
    return true;
}

static void
sc_window_ui_menu_hide(struct sc_window_ui_menu_data *data) {
    if (data->window
            && !(SDL_GetWindowFlags(data->window) & SDL_WINDOW_HIDDEN)) {
        SDL_HideWindow(data->window);
    }
    data->hovered = false;
}

static void
sc_window_ui_menu_show(struct sc_window_ui_menu_data *data) {
    if (!data->window && !sc_window_ui_menu_create(data)) {
        return;
    }
    sc_window_ui_menu_render(data);
    SDL_ShowWindow(data->window);
}

void
sc_window_ui_menu_init(struct sc_window_ui_menu *menu, SDL_Window *window) {
    struct sc_window_ui_menu_data *data = calloc(1, sizeof(*data));
    if (data) {
        data->parent = window;
    }
    menu->data = data;
}

void
sc_window_ui_menu_configure(struct sc_window_ui_menu *menu, bool visible,
                            bool shortcut_enabled,
                            sc_window_ui_toolbar_toggle_cb on_toggle,
                            void *userdata) {
    struct sc_window_ui_menu_data *data = menu->data;
    if (!data) {
        return;
    }
    data->toolbar_visible = visible;
    data->shortcut_enabled = shortcut_enabled;
    data->toolbar_toggle_callback = on_toggle;
    data->toolbar_toggle_userdata = userdata;
}

void
sc_window_ui_menu_set_toolbar_visible(struct sc_window_ui_menu *menu,
                                      bool visible) {
    struct sc_window_ui_menu_data *data = menu->data;
    if (!data) {
        return;
    }
    data->toolbar_visible = visible;
    if (data->window
            && !(SDL_GetWindowFlags(data->window) & SDL_WINDOW_HIDDEN)) {
        sc_window_ui_menu_render(data);
    }
}

static SDL_WindowID
sc_window_ui_menu_get_event_window_id(const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION:
            return event->motion.windowID;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return event->button.windowID;
        case SDL_EVENT_MOUSE_WHEEL:
            return event->wheel.windowID;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            return event->key.windowID;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            return event->window.windowID;
        default:
            return 0;
    }
}

bool
sc_window_ui_menu_handle_event(struct sc_window_ui_menu *menu,
                               const SDL_Event *event) {
    struct sc_window_ui_menu_data *data = menu->data;
    if (!data || !data->toolbar_toggle_callback) {
        return false;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        SDL_WindowID window_id = event->key.windowID;
        if (data->shortcut_enabled
                && (window_id == SDL_GetWindowID(data->parent)
                    || window_id == data->window_id)
                && sc_window_ui_menu_is_toggle_shortcut(&event->key)) {
            sc_window_ui_menu_hide(data);
            sc_window_ui_menu_toggle_toolbar(data);
            return true;
        }
        if (window_id == SDL_GetWindowID(data->parent)
                && event->key.key == SDLK_F10 && !event->key.repeat
                && !(event->key.mod & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT
                                     | SDL_KMOD_ALT | SDL_KMOD_GUI))) {
            if (data->window
                    && !(SDL_GetWindowFlags(data->window)
                         & SDL_WINDOW_HIDDEN)) {
                sc_window_ui_menu_hide(data);
            } else {
                sc_window_ui_menu_show(data);
            }
            return true;
        }
    }

    if (!data->window
            || sc_window_ui_menu_get_event_window_id(event)
                    != data->window_id) {
        return false;
    }

    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION: {
            bool hovered = event->motion.y >= SC_VIEW_MENU_TITLE_HEIGHT;
            if (data->hovered != hovered) {
                data->hovered = hovered;
                sc_window_ui_menu_render(data);
            }
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button == SDL_BUTTON_LEFT
                    && event->button.y >= SC_VIEW_MENU_TITLE_HEIGHT) {
                sc_window_ui_menu_hide(data);
                sc_window_ui_menu_toggle_toolbar(data);
            }
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                sc_window_ui_menu_hide(data);
            } else if (event->key.key == SDLK_F10 && !event->key.repeat) {
                sc_window_ui_menu_hide(data);
            } else if (event->key.key == SDLK_RETURN
                    || event->key.key == SDLK_SPACE) {
                sc_window_ui_menu_hide(data);
                sc_window_ui_menu_toggle_toolbar(data);
            }
            return true;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            sc_window_ui_menu_hide(data);
            return true;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            data->hovered = false;
            sc_window_ui_menu_render(data);
            return true;
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            sc_window_ui_menu_render(data);
            return true;
        default:
            return true;
    }
}

void
sc_window_ui_menu_destroy(struct sc_window_ui_menu *menu) {
    struct sc_window_ui_menu_data *data = menu->data;
    if (!data) {
        return;
    }
    if (data->renderer) {
        SDL_DestroyRenderer(data->renderer);
    }
    if (data->window) {
        SDL_DestroyWindow(data->window);
    }
    free(data);
    menu->data = NULL;
}
