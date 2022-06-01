#include "scrcpy.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <sys/time.h>
#include <SDL2/SDL.h>

#ifdef _WIN32
// not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>
#endif

#include "controller.h"
#include "decoder.h"
#include "demuxer.h"
#include "events.h"
#include "file_pusher.h"
#include "keyboard_inject.h"
#include "mouse_inject.h"
#include "recorder.h"
#include "screen.h"
#include "server.h"
#ifdef HAVE_USB
# include "usb/aoa_hid.h"
# include "usb/hid_keyboard.h"
# include "usb/hid_mouse.h"
# include "usb/usb.h"
#endif
#include "util/acksync.h"
#include "util/log.h"
#include "util/net.h"
#ifdef HAVE_V4L2
# include "v4l2_sink.h"
#endif

struct scrcpy {
    struct sc_server server;
    struct sc_screen screen;
    struct sc_demuxer demuxer;
    struct sc_decoder decoder;
    struct sc_recorder recorder;
#ifdef HAVE_V4L2
    struct sc_v4l2_sink v4l2_sink;
#endif
    struct sc_controller controller;
    struct sc_file_pusher file_pusher;
#ifdef HAVE_USB
    struct sc_usb usb;
    struct sc_aoa aoa;
    // sequence/ack helper to synchronize clipboard and Ctrl+v via HID
    struct sc_acksync acksync;
#endif
    union {
        struct sc_keyboard_inject keyboard_inject;
#ifdef HAVE_USB
        struct sc_hid_keyboard keyboard_hid;
#endif
    };
    union {
        struct sc_mouse_inject mouse_inject;
#ifdef HAVE_USB
        struct sc_hid_mouse mouse_hid;
#endif
    };
};

static inline void
push_event(uint32_t type, const char *name) {
    SDL_Event event;
    event.type = type;
    int ret = SDL_PushEvent(&event);
    if (ret < 0) {
        LOGE("Could not post %s event: %s", name, SDL_GetError());
        // What could we do?
    }
}
#define PUSH_EVENT(TYPE) push_event(TYPE, # TYPE)

#ifdef _WIN32
BOOL WINAPI windows_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT) {
        PUSH_EVENT(SDL_QUIT);
        return TRUE;
    }
    return FALSE;
}
#endif // _WIN32

static void
sdl_set_hints(const char *render_driver) {

    if (render_driver && !SDL_SetHint(SDL_HINT_RENDER_DRIVER, render_driver)) {
        LOGW("Could not set render driver");
    }

    // Linear filtering
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        LOGW("Could not enable linear filtering");
    }

    // Handle a click to gain focus as any other click
    if (!SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1")) {
        LOGW("Could not enable mouse focus clickthrough");
    }

#ifdef SCRCPY_SDL_HAS_HINT_TOUCH_MOUSE_EVENTS
    // Disable synthetic mouse events from touch events
    // Touch events with id SDL_TOUCH_MOUSEID are ignored anyway, but it is
    // better not to generate them in the first place.
    if (!SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0")) {
        LOGW("Could not disable synthetic mouse events");
    }
#endif

#ifdef SCRCPY_SDL_HAS_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
    // Disable compositor bypassing on X11
    if (!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0")) {
        LOGW("Could not disable X11 compositor bypass");
    }
#endif

    // Do not minimize on focus loss
    if (!SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0")) {
        LOGW("Could not disable minimize on focus loss");
    }
}

static void
sdl_configure(bool display, bool disable_screensaver) {
#ifdef _WIN32
    // Clean up properly on Ctrl+C on Windows
    bool ok = SetConsoleCtrlHandler(windows_ctrl_handler, TRUE);
    if (!ok) {
        LOGW("Could not set Ctrl+C handler");
    }
#endif // _WIN32

    if (!display) {
        return;
    }

    if (disable_screensaver) {
        SDL_DisableScreenSaver();
    } else {
        SDL_EnableScreenSaver();
    }
}

static enum scrcpy_exit_code
event_loop(struct scrcpy *s) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case EVENT_STREAM_STOPPED:
                LOGW("Device disconnected");
                return SCRCPY_EXIT_DISCONNECTED;
            case SDL_QUIT:
                LOGD("User requested to quit");
                return SCRCPY_EXIT_SUCCESS;
            default:
                sc_screen_handle_event(&s->screen, &event);
                break;
        }
    }
    return SCRCPY_EXIT_FAILURE;
}

// Return true on success, false on error
static bool
await_for_server(bool *connected) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                LOGD("User requested to quit");
                *connected = false;
                return true;
            case EVENT_SERVER_CONNECTION_FAILED:
                LOGE("Server connection failed");
                return false;
            case EVENT_SERVER_CONNECTED:
                LOGD("Server connected");
                *connected = true;
                return true;
            default:
                break;
        }
    }

    LOGE("SDL_WaitEvent() error: %s", SDL_GetError());
    return false;
}

static SDL_LogPriority
sdl_priority_from_av_level(int level) {
    switch (level) {
        case AV_LOG_PANIC:
        case AV_LOG_FATAL:
            return SDL_LOG_PRIORITY_CRITICAL;
        case AV_LOG_ERROR:
            return SDL_LOG_PRIORITY_ERROR;
        case AV_LOG_WARNING:
            return SDL_LOG_PRIORITY_WARN;
        case AV_LOG_INFO:
            return SDL_LOG_PRIORITY_INFO;
    }
    // do not forward others, which are too verbose
    return 0;
}

static void
av_log_callback(void *avcl, int level, const char *fmt, va_list vl) {
    (void) avcl;
    SDL_LogPriority priority = sdl_priority_from_av_level(level);
    if (priority == 0) {
        return;
    }

    size_t fmt_len = strlen(fmt);
    char *local_fmt = malloc(fmt_len + 10);
    if (!local_fmt) {
        LOG_OOM();
        return;
    }
    memcpy(local_fmt, "[FFmpeg] ", 9); // do not write the final '\0'
    memcpy(local_fmt + 9, fmt, fmt_len + 1); // include '\0'
    SDL_LogMessageV(SDL_LOG_CATEGORY_VIDEO, priority, local_fmt, vl);
    free(local_fmt);
}

static void
sc_demuxer_on_eos(struct sc_demuxer *demuxer, void *userdata) {
    (void) demuxer;
    (void) userdata;

    PUSH_EVENT(EVENT_STREAM_STOPPED);
}

static void
sc_server_on_connection_failed(struct sc_server *server, void *userdata) {
    (void) server;
    (void) userdata;

    PUSH_EVENT(EVENT_SERVER_CONNECTION_FAILED);
}

static void
sc_server_on_connected(struct sc_server *server, void *userdata) {
    (void) server;
    (void) userdata;

    PUSH_EVENT(EVENT_SERVER_CONNECTED);
}

static void
sc_server_on_disconnected(struct sc_server *server, void *userdata) {
    (void) server;
    (void) userdata;

    LOGD("Server disconnected");
    // Do nothing, the disconnection will be handled by the "stream stopped"
    // event
}

enum scrcpy_exit_code
scrcpy(struct scrcpy_options *options) {
    static struct scrcpy scrcpy;
    struct scrcpy *s = &scrcpy;

    // Minimal SDL initialization
    if (SDL_Init(SDL_INIT_EVENTS)) {
        LOGE("Could not initialize SDL: %s", SDL_GetError());
        return SCRCPY_EXIT_FAILURE;
    }

    atexit(SDL_Quit);

    enum scrcpy_exit_code ret = SCRCPY_EXIT_FAILURE;

    bool server_started = false;
    bool file_pusher_initialized = false;
    bool recorder_initialized = false;
#ifdef HAVE_V4L2
    bool v4l2_sink_initialized = false;
#endif
    bool demuxer_started = false;
#ifdef HAVE_USB
    bool aoa_hid_initialized = false;
    bool hid_keyboard_initialized = false;
    bool hid_mouse_initialized = false;
#endif
    bool controller_initialized = false;
    bool controller_started = false;
    bool screen_initialized = false;

    struct sc_acksync *acksync = NULL;

    struct sc_server_params params = {
        .req_serial = options->serial,
        .select_usb = options->select_usb,
        .select_tcpip = options->select_tcpip,
        .log_level = options->log_level,
        .crop = options->crop,
        .port_range = options->port_range,
        .tunnel_host = options->tunnel_host,
        .tunnel_port = options->tunnel_port,
        .max_size = options->max_size,
        .bit_rate = options->bit_rate,
        .max_fps = options->max_fps,
        .lock_video_orientation = options->lock_video_orientation,
        .control = options->control,
        .display_id = options->display_id,
        .show_touches = options->show_touches,
        .stay_awake = options->stay_awake,
        .codec_options = options->codec_options,
        .encoder_name = options->encoder_name,
        .force_adb_forward = options->force_adb_forward,
        .power_off_on_close = options->power_off_on_close,
        .clipboard_autosync = options->clipboard_autosync,
        .downsize_on_error = options->downsize_on_error,
        .tcpip = options->tcpip,
        .tcpip_dst = options->tcpip_dst,
        .cleanup = options->cleanup,
        .power_on = options->power_on,
    };

    static const struct sc_server_callbacks cbs = {
        .on_connection_failed = sc_server_on_connection_failed,
        .on_connected = sc_server_on_connected,
        .on_disconnected = sc_server_on_disconnected,
    };
    if (!sc_server_init(&s->server, &params, &cbs, NULL)) {
        return SCRCPY_EXIT_FAILURE;
    }

    if (!sc_server_start(&s->server)) {
        goto end;
    }

    server_started = true;

    if (options->display) {
        sdl_set_hints(options->render_driver);
    }

    // Initialize SDL video in addition if display is enabled
    if (options->display && SDL_Init(SDL_INIT_VIDEO)) {
        LOGE("Could not initialize SDL: %s", SDL_GetError());
        goto end;
    }

    sdl_configure(options->display, options->disable_screensaver);

    // Await for server without blocking Ctrl+C handling
    bool connected;
    if (!await_for_server(&connected)) {
        goto end;
    }

    if (!connected) {
        // This is not an error, user requested to quit
        ret = SCRCPY_EXIT_SUCCESS;
        goto end;
    }

    // It is necessarily initialized here, since the device is connected
    struct sc_server_info *info = &s->server.info;

    const char *serial = s->server.serial;
    assert(serial);

    struct sc_file_pusher *fp = NULL;

    if (options->display && options->control) {
        if (!sc_file_pusher_init(&s->file_pusher, serial,
                                 options->push_target)) {
            goto end;
        }
        fp = &s->file_pusher;
        file_pusher_initialized = true;
    }

    struct sc_decoder *dec = NULL;
    bool needs_decoder = options->display;
#ifdef HAVE_V4L2
    needs_decoder |= !!options->v4l2_device;
#endif
    if (needs_decoder) {
        sc_decoder_init(&s->decoder);
        dec = &s->decoder;
    }

    struct sc_recorder *rec = NULL;
    if (options->record_filename) {
        if (!sc_recorder_init(&s->recorder,
                              options->record_filename,
                              options->record_format,
                              info->frame_size)) {
            goto end;
        }
        rec = &s->recorder;
        recorder_initialized = true;
    }

    av_log_set_callback(av_log_callback);

    static const struct sc_demuxer_callbacks demuxer_cbs = {
        .on_eos = sc_demuxer_on_eos,
    };
    sc_demuxer_init(&s->demuxer, s->server.video_socket, &demuxer_cbs, NULL);

    if (dec) {
        sc_demuxer_add_sink(&s->demuxer, &dec->packet_sink);
    }

    if (rec) {
        sc_demuxer_add_sink(&s->demuxer, &rec->packet_sink);
    }

    struct sc_controller *controller = NULL;
    struct sc_key_processor *kp = NULL;
    struct sc_mouse_processor *mp = NULL;

    if (options->control) {
#ifdef HAVE_USB
        bool use_hid_keyboard =
            options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_HID;
        bool use_hid_mouse =
            options->mouse_input_mode == SC_MOUSE_INPUT_MODE_HID;
        if (use_hid_keyboard || use_hid_mouse) {
            bool ok = sc_acksync_init(&s->acksync);
            if (!ok) {
                goto end;
            }

            ok = sc_usb_init(&s->usb);
            if (!ok) {
                LOGE("Failed to initialize USB");
                sc_acksync_destroy(&s->acksync);
                goto aoa_hid_end;
            }

            assert(serial);
            struct sc_usb_device usb_device;
            ok = sc_usb_select_device(&s->usb, serial, &usb_device);
            if (!ok) {
                sc_usb_destroy(&s->usb);
                goto aoa_hid_end;
            }

            LOGI("USB device: %s (%04" PRIx16 ":%04" PRIx16 ") %s %s",
                 usb_device.serial, usb_device.vid, usb_device.pid,
                 usb_device.manufacturer, usb_device.product);

            ok = sc_usb_connect(&s->usb, usb_device.device, NULL, NULL);
            sc_usb_device_destroy(&usb_device);
            if (!ok) {
                LOGE("Failed to connect to USB device %s", serial);
                sc_usb_destroy(&s->usb);
                sc_acksync_destroy(&s->acksync);
                goto aoa_hid_end;
            }

            ok = sc_aoa_init(&s->aoa, &s->usb, &s->acksync);
            if (!ok) {
                LOGE("Failed to enable HID over AOA");
                sc_usb_disconnect(&s->usb);
                sc_usb_destroy(&s->usb);
                sc_acksync_destroy(&s->acksync);
                goto aoa_hid_end;
            }

            if (use_hid_keyboard) {
                if (sc_hid_keyboard_init(&s->keyboard_hid, &s->aoa)) {
                    hid_keyboard_initialized = true;
                    kp = &s->keyboard_hid.key_processor;
                } else {
                    LOGE("Could not initialize HID keyboard");
                }
            }

            if (use_hid_mouse) {
                if (sc_hid_mouse_init(&s->mouse_hid, &s->aoa)) {
                    hid_mouse_initialized = true;
                    mp = &s->mouse_hid.mouse_processor;
                } else {
                    LOGE("Could not initialized HID mouse");
                }
            }

            bool need_aoa = hid_keyboard_initialized || hid_mouse_initialized;

            if (!need_aoa || !sc_aoa_start(&s->aoa)) {
                sc_acksync_destroy(&s->acksync);
                sc_usb_disconnect(&s->usb);
                sc_usb_destroy(&s->usb);
                sc_aoa_destroy(&s->aoa);
                goto aoa_hid_end;
            }

            acksync = &s->acksync;

            aoa_hid_initialized = true;

aoa_hid_end:
            if (!aoa_hid_initialized) {
                if (hid_keyboard_initialized) {
                    sc_hid_keyboard_destroy(&s->keyboard_hid);
                    hid_keyboard_initialized = false;
                }
                if (hid_mouse_initialized) {
                    sc_hid_mouse_destroy(&s->mouse_hid);
                    hid_mouse_initialized = false;
                }
            }

            if (use_hid_keyboard && !hid_keyboard_initialized) {
                LOGE("Fallback to default keyboard injection method "
                     "(-K/--hid-keyboard ignored)");
                options->keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_INJECT;
            }

            if (use_hid_mouse && !hid_mouse_initialized) {
                LOGE("Fallback to default mouse injection method "
                     "(-M/--hid-mouse ignored)");
                options->mouse_input_mode = SC_MOUSE_INPUT_MODE_INJECT;
            }
        }
#else
        assert(options->keyboard_input_mode != SC_KEYBOARD_INPUT_MODE_HID);
        assert(options->mouse_input_mode != SC_MOUSE_INPUT_MODE_HID);
#endif

        // keyboard_input_mode may have been reset if HID mode failed
        if (options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_INJECT) {
            sc_keyboard_inject_init(&s->keyboard_inject, &s->controller,
                                    options->key_inject_mode,
                                    options->forward_key_repeat);
            kp = &s->keyboard_inject.key_processor;
        }

        // mouse_input_mode may have been reset if HID mode failed
        if (options->mouse_input_mode == SC_MOUSE_INPUT_MODE_INJECT) {
            sc_mouse_inject_init(&s->mouse_inject, &s->controller);
            mp = &s->mouse_inject.mouse_processor;
        }

        if (!sc_controller_init(&s->controller, s->server.control_socket,
                                acksync)) {
            goto end;
        }
        controller_initialized = true;

        if (!sc_controller_start(&s->controller)) {
            goto end;
        }
        controller_started = true;
        controller = &s->controller;

        if (options->turn_screen_off) {
            struct sc_control_msg msg;
            msg.type = SC_CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE;
            msg.set_screen_power_mode.mode = SC_SCREEN_POWER_MODE_OFF;

            if (!sc_controller_push_msg(&s->controller, &msg)) {
                LOGW("Could not request 'set screen power mode'");
            }
        }

    }

    // There is a controller if and only if control is enabled
    assert(options->control == !!controller);

    if (options->display) {
        const char *window_title =
            options->window_title ? options->window_title : info->device_name;

        struct sc_screen_params screen_params = {
            .controller = controller,
            .fp = fp,
            .kp = kp,
            .mp = mp,
            .forward_all_clicks = options->forward_all_clicks,
            .legacy_paste = options->legacy_paste,
            .clipboard_autosync = options->clipboard_autosync,
            .shortcut_mods = &options->shortcut_mods,
            .window_title = window_title,
            .frame_size = info->frame_size,
            .always_on_top = options->always_on_top,
            .window_x = options->window_x,
            .window_y = options->window_y,
            .window_width = options->window_width,
            .window_height = options->window_height,
            .window_borderless = options->window_borderless,
            .rotation = options->rotation,
            .mipmaps = options->mipmaps,
            .fullscreen = options->fullscreen,
            .start_fps_counter = options->start_fps_counter,
            .buffering_time = options->display_buffer,
        };

        if (!sc_screen_init(&s->screen, &screen_params)) {
            goto end;
        }
        screen_initialized = true;

        sc_decoder_add_sink(&s->decoder, &s->screen.frame_sink);
    }

#ifdef HAVE_V4L2
    if (options->v4l2_device) {
        if (!sc_v4l2_sink_init(&s->v4l2_sink, options->v4l2_device,
                               info->frame_size, options->v4l2_buffer)) {
            goto end;
        }

        sc_decoder_add_sink(&s->decoder, &s->v4l2_sink.frame_sink);

        v4l2_sink_initialized = true;
    }
#endif

    // now we consumed the header values, the socket receives the video stream
    // start the demuxer
    if (!sc_demuxer_start(&s->demuxer)) {
        goto end;
    }
    demuxer_started = true;

    ret = event_loop(s);
    LOGD("quit...");

    // Close the window immediately on closing, because screen_destroy() may
    // only be called once the demuxer thread is joined (it may take time)
    sc_screen_hide_window(&s->screen);

end:
    // The demuxer is not stopped explicitly, because it will stop by itself on
    // end-of-stream
#ifdef HAVE_USB
    if (aoa_hid_initialized) {
        if (hid_keyboard_initialized) {
            sc_hid_keyboard_destroy(&s->keyboard_hid);
        }
        if (hid_mouse_initialized) {
            sc_hid_mouse_destroy(&s->mouse_hid);
        }
        sc_aoa_stop(&s->aoa);
        sc_usb_stop(&s->usb);
    }
    if (acksync) {
        sc_acksync_destroy(acksync);
    }
#endif
    if (controller_started) {
        sc_controller_stop(&s->controller);
    }
    if (file_pusher_initialized) {
        sc_file_pusher_stop(&s->file_pusher);
    }
    if (screen_initialized) {
        sc_screen_interrupt(&s->screen);
    }

    if (server_started) {
        // shutdown the sockets and kill the server
        sc_server_stop(&s->server);
    }

    // now that the sockets are shutdown, the demuxer and controller are
    // interrupted, we can join them
    if (demuxer_started) {
        sc_demuxer_join(&s->demuxer);
    }

#ifdef HAVE_V4L2
    if (v4l2_sink_initialized) {
        sc_v4l2_sink_destroy(&s->v4l2_sink);
    }
#endif

#ifdef HAVE_USB
    if (aoa_hid_initialized) {
        sc_aoa_join(&s->aoa);
        sc_aoa_destroy(&s->aoa);
        sc_usb_join(&s->usb);
        sc_usb_disconnect(&s->usb);
        sc_usb_destroy(&s->usb);
    }
#endif

    // Destroy the screen only after the demuxer is guaranteed to be finished,
    // because otherwise the screen could receive new frames after destruction
    if (screen_initialized) {
        sc_screen_join(&s->screen);
        sc_screen_destroy(&s->screen);
    }

    if (controller_started) {
        sc_controller_join(&s->controller);
    }
    if (controller_initialized) {
        sc_controller_destroy(&s->controller);
    }

    if (recorder_initialized) {
        sc_recorder_destroy(&s->recorder);
    }

    if (file_pusher_initialized) {
        sc_file_pusher_join(&s->file_pusher);
        sc_file_pusher_destroy(&s->file_pusher);
    }

    sc_server_destroy(&s->server);

    return ret;
}
