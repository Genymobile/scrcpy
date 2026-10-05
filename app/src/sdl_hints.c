#include "sdl_hints.h"

#include "util/log.h"

void
sc_sdl_set_hints(const char *render_driver, bool disable_screensaver) {
    if (render_driver && !SDL_SetHint(SDL_HINT_RENDER_DRIVER, render_driver)) {
        LOGW("Could not set render driver");
    }

    // App name used in various contexts (such as PulseAudio)
    if (!SDL_SetHint(SDL_HINT_APP_NAME, "scrcpy")) {
        LOGW("Could not set app name");
    }

    // Handle a click to gain focus as any other click
    if (!SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1")) {
        LOGW("Could not enable mouse focus clickthrough");
    }

    // Disable synthetic mouse events from touch events
    // Touch events with id SDL_TOUCH_MOUSEID are ignored anyway, but it is
    // better not to generate them in the first place.
    if (!SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0")) {
        LOGW("Could not disable synthetic mouse events");
    }

    // Disable compositor bypassing on X11
    if (!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0")) {
        LOGW("Could not disable X11 compositor bypass");
    }

    // Do not minimize on focus loss
    if (!SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0")) {
        LOGW("Could not disable minimize on focus loss");
    }

    if (!SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1")) {
        LOGW("Could not allow joystick background events");
    }

    if (!disable_screensaver
            && !SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1")) {
        LOGW("Could not enable screensaver");
    }

    // On X11, SDL creates the OpenGL contexts with GLX by default, but
    // importing hardware decoded frames (VA-API) requires EGL. Always use EGL,
    // so that the rendering does not depend on the hardware decoding mode. It
    // has no effect on other platforms.
    // The environment variable SDL_VIDEO_FORCE_EGL=0 takes priority to restore
    // GLX (hardware decoding is then unavailable).
    if (!SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1")) {
        LOGW("Could not force EGL");
    }

    // On Windows, SDL creates the Direct3D 11 device single-threaded by
    // default, but hardware decoding (D3D11VA) uses it from the decoder thread.
    // Always create it thread-safe, so that the rendering does not depend on
    // the hardware decoding mode. It has no effect on other platforms.
    if (!SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1")) {
        LOGW("Could not enable Direct3D thread safety");
    }
}

