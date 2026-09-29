#include "window_ui_menu.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <SDL3/SDL_system.h>

#include "util/log.h"

#define SC_WIN_VIEW_MENU_ID 0x5343

struct sc_window_ui_menu_data {
    SDL_Window *window;
    HWND hwnd;
    HMENU menu_bar;
    HMENU view_menu;
    bool owns_menu_bar;
    bool toolbar_visible;
    bool shortcut_enabled;
    sc_window_ui_toolbar_toggle_cb toolbar_toggle_callback;
    void *toolbar_toggle_userdata;
};

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

static const wchar_t *
sc_window_ui_menu_get_label(bool visible, bool shortcut_enabled) {
    if (shortcut_enabled) {
        return visible ? L"&Hide Toolbar\tCtrl+Shift+T"
                       : L"&Show Toolbar\tCtrl+Shift+T";
    }
    return visible ? L"&Hide Toolbar" : L"&Show Toolbar";
}

static bool SDLCALL
sc_window_ui_menu_message_hook(void *userdata, MSG *msg) {
    struct sc_window_ui_menu_data *data = userdata;
    if (msg->hwnd == data->hwnd && msg->message == WM_COMMAND
            && LOWORD(msg->wParam) == SC_WIN_VIEW_MENU_ID) {
        sc_window_ui_menu_toggle_toolbar(data);
        return false;
    }
    return true;
}

static void
sc_window_ui_menu_remove_native(struct sc_window_ui_menu_data *data) {
    if (!data->view_menu) {
        return;
    }

    SDL_SetWindowsMessageHook(NULL, NULL);
    if (data->owns_menu_bar) {
        SetMenu(data->hwnd, NULL);
        DestroyMenu(data->menu_bar);
    } else {
        int count = GetMenuItemCount(data->menu_bar);
        for (int i = 0; i < count; ++i) {
            if (GetSubMenu(data->menu_bar, i) == data->view_menu) {
                RemoveMenu(data->menu_bar, i, MF_BYPOSITION);
                break;
            }
        }
        DestroyMenu(data->view_menu);
        DrawMenuBar(data->hwnd);
    }

    data->view_menu = NULL;
    data->menu_bar = NULL;
    data->hwnd = NULL;
}

static bool
sc_window_ui_menu_create_native(struct sc_window_ui_menu_data *data) {
    SDL_PropertiesID props = SDL_GetWindowProperties(data->window);
    data->hwnd = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (!data->hwnd) {
        LOGW("Could not get native Windows handle for toolbar menu");
        return false;
    }

    data->menu_bar = GetMenu(data->hwnd);
    if (!data->menu_bar) {
        data->menu_bar = CreateMenu();
        data->owns_menu_bar = true;
    }
    data->view_menu = CreatePopupMenu();
    if (!data->menu_bar || !data->view_menu) {
        LOGW("Could not create native Windows toolbar menu");
        if (data->view_menu) {
            DestroyMenu(data->view_menu);
        }
        if (data->owns_menu_bar && data->menu_bar) {
            DestroyMenu(data->menu_bar);
        }
        data->view_menu = NULL;
        data->menu_bar = NULL;
        data->hwnd = NULL;
        data->owns_menu_bar = false;
        return false;
    }

    const wchar_t *label =
        sc_window_ui_menu_get_label(data->toolbar_visible,
                                    data->shortcut_enabled);
    if (!AppendMenuW(data->view_menu, MF_STRING, SC_WIN_VIEW_MENU_ID, label)
            || !AppendMenuW(data->menu_bar, MF_POPUP,
                            (UINT_PTR) data->view_menu, L"&View")) {
        LOGW("Could not populate native Windows toolbar menu");
        sc_window_ui_menu_remove_native(data);
        return false;
    }

    if (data->owns_menu_bar && !SetMenu(data->hwnd, data->menu_bar)) {
        LOGW("Could not attach native Windows toolbar menu");
        sc_window_ui_menu_remove_native(data);
        return false;
    }

    DrawMenuBar(data->hwnd);
    SDL_SetWindowsMessageHook(sc_window_ui_menu_message_hook, data);
    return true;
}

void
sc_window_ui_menu_init(struct sc_window_ui_menu *menu, SDL_Window *window) {
    struct sc_window_ui_menu_data *data = calloc(1, sizeof(*data));
    if (data) {
        data->window = window;
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
    sc_window_ui_menu_create_native(data);
}

void
sc_window_ui_menu_set_toolbar_visible(struct sc_window_ui_menu *menu,
                                      bool visible) {
    struct sc_window_ui_menu_data *data = menu->data;
    if (!data) {
        return;
    }
    data->toolbar_visible = visible;
    if (data->view_menu) {
        const wchar_t *label =
            sc_window_ui_menu_get_label(visible, data->shortcut_enabled);
        ModifyMenuW(data->view_menu, SC_WIN_VIEW_MENU_ID,
                    MF_BYCOMMAND | MF_STRING, SC_WIN_VIEW_MENU_ID, label);
        DrawMenuBar(data->hwnd);
    }
}

bool
sc_window_ui_menu_handle_event(struct sc_window_ui_menu *menu,
                               const SDL_Event *event) {
    struct sc_window_ui_menu_data *data = menu->data;
    if (!data || !data->toolbar_toggle_callback) {
        return false;
    }
    if (data->shortcut_enabled && event->type == SDL_EVENT_KEY_DOWN
            && event->key.windowID == SDL_GetWindowID(data->window)
            && sc_window_ui_menu_is_toggle_shortcut(&event->key)) {
        sc_window_ui_menu_toggle_toolbar(data);
        return true;
    }
    return false;
}

void
sc_window_ui_menu_destroy(struct sc_window_ui_menu *menu) {
    struct sc_window_ui_menu_data *data = menu->data;
    if (!data) {
        return;
    }
    sc_window_ui_menu_remove_native(data);
    free(data);
    menu->data = NULL;
}
