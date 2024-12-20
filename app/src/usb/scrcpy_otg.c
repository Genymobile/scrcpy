#include "scrcpy_otg.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#ifdef _WIN32
# include "adb/adb.h"
#endif
#include "events.h"
#include "usb/screen_otg.h"
#include "usb/aoa_hid.h"
#include "usb/gamepad_aoa.h"
#include "usb/keyboard_aoa.h"
#include "usb/mouse_aoa.h"
#include "util/log.h"

struct scrcpy_otg {
    struct sc_usb usb;
    struct sc_aoa aoa;
    struct sc_keyboard_aoa keyboard;
    struct sc_mouse_aoa mouse;
    struct sc_gamepad_aoa gamepad;

    struct sc_screen_otg screen_otg;
};

static void
sc_usb_on_disconnected(struct sc_usb *usb, void *userdata) {
    (void) usb;
    (void) userdata;

    sc_push_event(SC_EVENT_USB_DEVICE_DISCONNECTED);
}

static enum scrcpy_exit_code
event_loop(struct scrcpy_otg *s) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SC_EVENT_USB_DEVICE_DISCONNECTED:
                LOGW("Device disconnected");
                return SCRCPY_EXIT_DISCONNECTED;
            case SC_EVENT_AOA_OPEN_ERROR:
                LOGE("AOA open error");
                return SCRCPY_EXIT_FAILURE;
            case SDL_QUIT:
                LOGD("User requested to quit");
                return SCRCPY_EXIT_SUCCESS;
            default:
                sc_screen_otg_handle_event(&s->screen_otg, &event);
                break;
        }
    }
    return SCRCPY_EXIT_FAILURE;
}

enum scrcpy_exit_code
scrcpy_otg(struct scrcpy_options *options) {
    static struct scrcpy_otg scrcpy_otg;
    struct scrcpy_otg *s = &scrcpy_otg;

    const char *serial = options->serial;

    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        LOGW("Could not enable linear filtering");
    }

    if (!SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1")) {
        LOGW("Could not allow joystick background events");
    }

    // Minimal SDL initialization
    if (SDL_Init(SDL_INIT_EVENTS)) {
        LOGE("Could not initialize SDL: %s", SDL_GetError());
        return SCRCPY_EXIT_FAILURE;
    }

    if (options->gamepad_input_mode != SC_GAMEPAD_INPUT_MODE_DISABLED) {
        if (SDL_Init(SDL_INIT_GAMECONTROLLER)) {
            LOGE("Could not initialize SDL controller: %s", SDL_GetError());
            // Not fatal, keyboard/mouse should still work
        }
    }

    atexit(SDL_Quit);

    if (!SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1")) {
        LOGW("Could not enable mouse focus clickthrough");
    }

    enum scrcpy_exit_code ret = SCRCPY_EXIT_FAILURE;

    struct sc_keyboard_aoa *keyboard = NULL;
    struct sc_mouse_aoa *mouse = NULL;
    struct sc_gamepad_aoa *gamepad = NULL;
    bool usb_device_initialized = false;
    bool usb_connected = false;
    bool aoa_started = false;
    bool aoa_initialized = false;

#ifdef _WIN32
    // On Windows, only one process could open a USB device
    // <https://github.com/Genymobile/scrcpy/issues/2773>
    LOGI("Killing adb server (if any)...");
    if (sc_adb_init()) {
        unsigned flags = SC_ADB_NO_STDOUT | SC_ADB_NO_STDERR | SC_ADB_NO_LOGERR;
        // uninterruptible (intr == NULL), but in practice it's very quick
        sc_adb_kill_server(NULL, flags);
        sc_adb_destroy();
    } else {
        LOGW("Could not call adb executable, adb server not killed");
    }
#endif

    static const struct sc_usb_callbacks cbs = {
        .on_disconnected = sc_usb_on_disconnected,
    };
    bool ok = sc_usb_init(&s->usb);
    if (!ok) {
        return SCRCPY_EXIT_FAILURE;
    }

    struct sc_usb_device usb_device;
    ok = sc_usb_select_device(&s->usb, serial, &usb_device);
    if (!ok) {
        goto end;
    }

    usb_device_initialized = true;

    ok = sc_usb_connect(&s->usb, usb_device.device, &cbs, NULL);
    if (!ok) {
        goto end;
    }
    usb_connected = true;

    ok = sc_aoa_init(&s->aoa, &s->usb, NULL);
    if (!ok) {
        goto end;
    }
    aoa_initialized = true;

    assert(options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_AOA
        || options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_DISABLED);
    assert(options->mouse_input_mode == SC_MOUSE_INPUT_MODE_AOA
        || options->mouse_input_mode == SC_MOUSE_INPUT_MODE_DISABLED);
    assert(options->gamepad_input_mode == SC_GAMEPAD_INPUT_MODE_AOA
        || options->gamepad_input_mode == SC_GAMEPAD_INPUT_MODE_DISABLED);

    bool enable_keyboard =
        options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_AOA;
    bool enable_mouse =
        options->mouse_input_mode == SC_MOUSE_INPUT_MODE_AOA;
    bool enable_gamepad =
        options->gamepad_input_mode == SC_GAMEPAD_INPUT_MODE_AOA;

    if (enable_keyboard) {
        ok = sc_keyboard_aoa_init(&s->keyboard, &s->aoa);
        if (!ok) {
            goto end;
        }
        keyboard = &s->keyboard;
    }

    if (enable_mouse) {
        ok = sc_mouse_aoa_init(&s->mouse, &s->aoa);
        if (!ok) {
            goto end;
        }
        mouse = &s->mouse;
    }

    if (enable_gamepad) {
        sc_gamepad_aoa_init(&s->gamepad, &s->aoa);
        gamepad = &s->gamepad;
    }

    ok = sc_aoa_start(&s->aoa);
    if (!ok) {
        goto end;
    }
    aoa_started = true;

    const char *window_title = options->window_title;
    if (!window_title) {
        window_title = usb_device.product ? usb_device.product : "scrcpy";
    }

    struct sc_screen_otg_params params = {
        .keyboard = keyboard,
        .mouse = mouse,
        .gamepad = gamepad,
        .window_title = window_title,
        .always_on_top = options->always_on_top,
        .window_x = options->window_x,
        .window_y = options->window_y,
        .window_width = options->window_width,
        .window_height = options->window_height,
        .window_borderless = options->window_borderless,
        .shortcut_mods = options->shortcut_mods,
    };

    ok = sc_screen_otg_init(&s->screen_otg, &params);
    if (!ok) {
        goto end;
    }

    // usb_device not needed anymore
    sc_usb_device_destroy(&usb_device);
    usb_device_initialized = false;

    ret = event_loop(s);
    LOGD("quit...");

end:
    if (aoa_started) {
        sc_aoa_stop(&s->aoa);
    }
    sc_usb_stop(&s->usb);

    if (mouse) {
        sc_mouse_aoa_destroy(&s->mouse);
    }
    if (keyboard) {
        sc_keyboard_aoa_destroy(&s->keyboard);
    }
    if (gamepad) {
        sc_gamepad_aoa_destroy(&s->gamepad);
    }

    if (aoa_initialized) {
        sc_aoa_join(&s->aoa);
        sc_aoa_destroy(&s->aoa);
    }

    sc_usb_join(&s->usb);

    if (usb_connected) {
        sc_usb_disconnect(&s->usb);
    }

    if (usb_device_initialized) {
        sc_usb_device_destroy(&usb_device);
    }

    sc_usb_destroy(&s->usb);

    return ret;
}
