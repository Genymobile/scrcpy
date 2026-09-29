#include "window_ui.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "window_ui_menu.h"

#define SC_TOAST_WIDTH 184
#define SC_TOAST_HEIGHT 38
#define SC_TOAST_RADIUS 12.f

struct sc_window_ui_data {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_TimerID timer;
    struct sc_window_ui_menu menu;
};

static void
sc_window_ui_fill_rounded_rect(SDL_Renderer *renderer, SDL_FRect rect,
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

static void SDLCALL
sc_window_ui_hide_toast(void *userdata) {
    struct sc_window_ui_data *data = userdata;
    if (data->window) {
        SDL_HideWindow(data->window);
    }
}

static Uint32 SDLCALL
sc_window_ui_toast_timer(void *userdata, SDL_TimerID timer_id,
                         Uint32 interval) {
    (void) timer_id;
    (void) interval;
    struct sc_window_ui_data *data = userdata;
    data->timer = 0;
    SDL_RunOnMainThread(sc_window_ui_hide_toast, data, false);
    return 0;
}

static bool
sc_window_ui_create_toast(struct sc_window_ui *ui) {
    struct sc_window_ui_data *data = ui->data;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return false;
    }

    bool ok =
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                              "scrcpy toast");
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
                                SC_TOAST_WIDTH);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
                                SC_TOAST_HEIGHT);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN,
                                 true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN,
                                 true);
    ok &= SDL_SetBooleanProperty(
        props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_UTILITY_BOOLEAN, true);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN,
                                 false);
    ok &= SDL_SetBooleanProperty(props,
                                 SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN,
                                 true);
    ok &= SDL_SetPointerProperty(props,
                                 SDL_PROP_WINDOW_CREATE_PARENT_POINTER,
                                 ui->window);
    if (ok) {
        data->window = SDL_CreateWindowWithProperties(props);
    }
    SDL_DestroyProperties(props);
    if (!data->window) {
        return false;
    }

    data->renderer = SDL_CreateRenderer(data->window, NULL);
    if (!data->renderer) {
        SDL_DestroyWindow(data->window);
        data->window = NULL;
        return false;
    }
    return true;
}

void
sc_window_ui_init(struct sc_window_ui *ui, SDL_Window *window) {
    ui->window = window;
    ui->data = calloc(1, sizeof(struct sc_window_ui_data));
    if (ui->data) {
        struct sc_window_ui_data *data = ui->data;
        sc_window_ui_menu_init(&data->menu, window);
    }
}

void
sc_window_ui_set_device_title(struct sc_window_ui *ui, const char *primary,
                              const char *secondary) {
    (void) ui;
    (void) primary;
    (void) secondary;
}

void
sc_window_ui_show_toast(struct sc_window_ui *ui, const char *message) {
    struct sc_window_ui_data *data = ui->data;
    if (!data || (!data->window && !sc_window_ui_create_toast(ui))) {
        return;
    }

    int parent_x;
    int parent_y;
    int parent_width;
    int parent_height;
    if (SDL_GetWindowPosition(ui->window, &parent_x, &parent_y)
            && SDL_GetWindowSize(ui->window, &parent_width, &parent_height)) {
        SDL_SetWindowPosition(data->window,
                              parent_x + (parent_width - SC_TOAST_WIDTH) / 2,
                              parent_y + parent_height - 58);
    }

    float density = SDL_GetWindowPixelDensity(data->window);
    if (!density) {
        density = 1.f;
    }
    SDL_SetRenderScale(data->renderer, density, density);
    SDL_SetRenderDrawColor(data->renderer, 0, 0, 0, 0);
    SDL_RenderClear(data->renderer);
    SDL_SetRenderDrawColor(data->renderer, 20, 20, 20, 230);
    sc_window_ui_fill_rounded_rect(
        data->renderer,
        (SDL_FRect) {0, 0, SC_TOAST_WIDTH, SC_TOAST_HEIGHT},
        SC_TOAST_RADIUS);
    SDL_SetRenderDrawColor(data->renderer, 255, 255, 255, 255);
    float text_width = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * strlen(message);
    SDL_RenderDebugText(data->renderer,
                        (SC_TOAST_WIDTH - text_width) / 2.f,
                        (SC_TOAST_HEIGHT
                         - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.f,
                        message);
    SDL_RenderPresent(data->renderer);
    SDL_ShowWindow(data->window);

    if (data->timer) {
        SDL_RemoveTimer(data->timer);
    }
    data->timer = SDL_AddTimer(2000, sc_window_ui_toast_timer, data);
}

void
sc_window_ui_configure_toolbar_menu(struct sc_window_ui *ui, bool visible,
                                    bool shortcut_enabled,
                                    sc_window_ui_toolbar_toggle_cb on_toggle,
                                    void *userdata) {
    struct sc_window_ui_data *data = ui->data;
    if (data) {
        sc_window_ui_menu_configure(&data->menu, visible, shortcut_enabled,
                                    on_toggle, userdata);
    }
}

void
sc_window_ui_set_toolbar_menu_visible(struct sc_window_ui *ui, bool visible) {
    struct sc_window_ui_data *data = ui->data;
    if (data) {
        sc_window_ui_menu_set_toolbar_visible(&data->menu, visible);
    }
}

bool
sc_window_ui_handle_event(struct sc_window_ui *ui, const SDL_Event *event) {
    struct sc_window_ui_data *data = ui->data;
    return data && sc_window_ui_menu_handle_event(&data->menu, event);
}

void
sc_window_ui_destroy(struct sc_window_ui *ui) {
    struct sc_window_ui_data *data = ui->data;
    if (!data) {
        return;
    }
    if (data->timer) {
        SDL_RemoveTimer(data->timer);
    }
    sc_window_ui_menu_destroy(&data->menu);
    if (data->renderer) {
        SDL_DestroyRenderer(data->renderer);
    }
    if (data->window) {
        SDL_DestroyWindow(data->window);
    }
    free(data);
    ui->data = NULL;
    ui->window = NULL;
}
