#include "win_quick_toggle.h"

#ifdef _WIN32

#include <windows.h>

#include "util/log.h"

// Default hotkey: Ctrl + ` (OEM_3)
// RegisterHotKey MOD: use Ctrl only.

static const UINT sc_hotkey_id = 0x5343; // 'SC'
static const UINT sc_hotkey_mod = MOD_CONTROL;
static const UINT sc_hotkey_vk = VK_OEM_3; // ` or ~

static SDL_Window *sc_window;
static bool sc_registered;
static bool sc_visible;

static void
sc_win_set_toolwindow(HWND hwnd) {
    // Make it behave like a tool window:
    // - does not appear in the taskbar (typical behavior)
    // - also behaves better for focus.
    LONG_PTR ex_style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (!ex_style) {
        return;
    }

    ex_style |= WS_EX_TOOLWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_style);

    // Also avoid forcing it as an app window.
    // (WS_EX_APPWINDOW can re-add a taskbar button in some cases.)
    ex_style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex_style &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_style);
}

static void
sc_win_focus_window(HWND hwnd) {
    // Try to focus without stealing focus aggressively.
    // Use SetForegroundWindow if allowed.
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    SetForegroundWindow(hwnd);

    // SDL uses ShowWindow/HideWindow for visibility.
    // Ensure keyboard focus.
    SetFocus(hwnd);
}

bool
sc_win_quick_toggle_init(SDL_Window *window) {
    sc_window = window;
    sc_visible = true;
    sc_registered = false;

    if (!sc_window) {
        LOGW("Quick toggle: no SDL window");
        return false;
    }

    // Retrieve the native HWND from SDL.
    // SDL3 exposes it via SDL_GetPointerProperty with SDL_PROP_WINDOW_HWND.
    // If this fails, we can still register the hotkey but cannot set tool
    // window style.
    HWND hwnd = NULL;

    SDL_PropertiesID props = SDL_GetWindowProperties(sc_window);
    if (props) {
        // The property name exists on Windows builds of SDL3.
        // It returns a pointer-sized integer casted from HWND.
        //
        // If SDL changes, this will fail safely.
        hwnd = (HWND)(intptr_t) SDL_GetPointerProperty(props, SDL_PROP_WINDOW_HWND, NULL);
    }

    if (hwnd) {
        sc_win_set_toolwindow(hwnd);
    }

    // Register global hotkey.
    // Using ` only works for US keyboard layout; it matches requirement default
    // Ctrl + `.
    BOOL ok = RegisterHotKey(NULL, sc_hotkey_id, sc_hotkey_mod, sc_hotkey_vk);
    if (!ok) {
        LOGW("Could not register quick-toggle hotkey: %lu", GetLastError());
        // Not fatal: feature just disabled.
        return false;
    }

    sc_registered = true;
    return true;
}

void
sc_win_quick_toggle_destroy(void) {
    if (sc_registered) {
        UnregisterHotKey(NULL, sc_hotkey_id);
    }
    sc_registered = false;
    sc_window = NULL;
}

// Called from the main loop to handle WM_HOTKEY.
// Returns true if WM_HOTKEY was processed.
bool
sc_win_quick_toggle_handle_msg(const MSG *msg) {

    if (!msg) {
        return false;
    }
    if (msg->message != WM_HOTKEY) {
        return false;
    }
    if ((UINT) msg->wParam != sc_hotkey_id) {
        return false;
    }

    if (!sc_window) {
        return true;
    }

    // Toggle visibility.
    sc_visible = !sc_visible;
    if (sc_visible) {
        // Show and focus.
        SDL_ShowWindow(sc_window);

        // Focus best-effort.
        SDL_PropertiesID props = SDL_GetWindowProperties(sc_window);
        HWND hwnd = NULL;
        if (props) {
            hwnd = (HWND)(intptr_t) SDL_GetPointerProperty(props, SDL_PROP_WINDOW_HWND, NULL);
        }
        if (hwnd) {
            sc_win_focus_window(hwnd);
        }
    } else {
        SDL_HideWindow(sc_window);
    }

    return true;
}

#endif // _WIN32

