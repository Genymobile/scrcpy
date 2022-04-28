#include "scrcpy_otg.h"

#include <SDL2/SDL.h>

#include "adb/adb.h"
#include "events.h"
#include "screen_otg.h"
#include "util/log.h"

struct scrcpy_otg {
    struct sc_usb usb;
    struct sc_aoa aoa;
    struct sc_hid_keyboard keyboard;
    struct sc_hid_mouse mouse;

    struct sc_screen_otg screen_otg;
};

static void
sc_usb_on_disconnected(struct sc_usb *usb, void *userdata) {
    (void) usb;
    (void) userdata;

    SDL_Event event;
    event.type = EVENT_USB_DEVICE_DISCONNECTED;
    int ret = SDL_PushEvent(&event);
    if (ret < 0) {
        LOGE("Could not post USB disconnection event: %s", SDL_GetError());
    }
}

static enum scrcpy_exit_code
event_loop(struct scrcpy_otg *s) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case EVENT_USB_DEVICE_DISCONNECTED:
                LOGW("Device disconnected");
                return SCRCPY_EXIT_DISCONNECTED;
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

    // Minimal SDL initialization
    if (SDL_Init(SDL_INIT_EVENTS)) {
        LOGE("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }

    atexit(SDL_Quit);

    if (!SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1")) {
        LOGW("Could not enable mouse focus clickthrough");
    }

    enum scrcpy_exit_code ret = SCRCPY_EXIT_FAILURE;

    struct sc_hid_keyboard *keyboard = NULL;
    struct sc_hid_mouse *mouse = NULL;
    bool usb_device_initialized = false;
    bool usb_connected = false;
    bool aoa_started = false;
    bool aoa_initialized = false;

#ifdef _WIN32
    // On Windows, only one process could open a USB device
    // <https://github.com/Genymobile/scrcpy/issues/2773>
    LOGI("Killing adb daemon (if any)...");
    unsigned flags = SC_ADB_NO_STDOUT | SC_ADB_NO_STDERR | SC_ADB_NO_LOGERR;
    // uninterruptible (intr == NULL), but in practice it's very quick
    sc_adb_kill_server(NULL, flags);
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

    LOGI("USB device: %s (%04x:%04x) %s %s", usb_device.serial,
         (unsigned) usb_device.vid, (unsigned) usb_device.pid,
         usb_device.manufacturer, usb_device.product);

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

    bool enable_keyboard =
        options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_HID;
    bool enable_mouse =
        options->mouse_input_mode == SC_MOUSE_INPUT_MODE_HID;

    // If neither --hid-keyboard or --hid-mouse is passed, enable both
    if (!enable_keyboard && !enable_mouse) {
        enable_keyboard = true;
        enable_mouse = true;
    }

    if (enable_keyboard) {
        ok = sc_hid_keyboard_init(&s->keyboard, &s->aoa);
        if (!ok) {
            goto end;
        }
        keyboard = &s->keyboard;
    }

    if (enable_mouse) {
        ok = sc_hid_mouse_init(&s->mouse, &s->aoa);
        if (!ok) {
            goto end;
        }
        mouse = &s->mouse;
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
        .window_title = window_title,
        .always_on_top = options->always_on_top,
        .window_x = options->window_x,
        .window_y = options->window_y,
        .window_width = options->window_width,
        .window_height = options->window_height,
        .window_borderless = options->window_borderless,
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
        sc_hid_mouse_destroy(&s->mouse);
    }
    if (keyboard) {
        sc_hid_keyboard_destroy(&s->keyboard);
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
