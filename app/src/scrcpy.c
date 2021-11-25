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
#include "events.h"
#include "file_handler.h"
#include "input_manager.h"
#ifdef HAVE_AOA_HID
# include "hid_keyboard.h"
#endif
#include "keyboard_inject.h"
#include "mouse_inject.h"
#include "recorder.h"
#include "screen.h"
#include "server.h"
#include "stream.h"
#include "util/acksync.h"
#include "util/log.h"
#include "util/net.h"
#ifdef HAVE_V4L2
# include "v4l2_sink.h"
#endif

struct scrcpy {
    struct sc_server server;
    struct screen screen;
    struct stream stream;
    struct decoder decoder;
    struct recorder recorder;
#ifdef HAVE_V4L2
    struct sc_v4l2_sink v4l2_sink;
#endif
    struct controller controller;
    struct file_handler file_handler;
#ifdef HAVE_AOA_HID
    struct sc_aoa aoa;
    // sequence/ack helper to synchronize clipboard and Ctrl+v via HID
    struct sc_acksync acksync;
#endif
    union {
        struct sc_keyboard_inject keyboard_inject;
#ifdef HAVE_AOA_HID
        struct sc_hid_keyboard keyboard_hid;
#endif
    };
    struct sc_mouse_inject mouse_inject;
    struct input_manager input_manager;
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

#ifdef SCRCPY_SDL_HAS_HINT_MOUSE_FOCUS_CLICKTHROUGH
    // Handle a click to gain focus as any other click
    if (!SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1")) {
        LOGW("Could not enable mouse focus clickthrough");
    }
#endif

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
        LOGD("Screensaver disabled");
        SDL_DisableScreenSaver();
    } else {
        LOGD("Screensaver enabled");
        SDL_EnableScreenSaver();
    }
}

static bool
is_apk(const char *file) {
    const char *ext = strrchr(file, '.');
    return ext && !strcmp(ext, ".apk");
}

enum event_result {
    EVENT_RESULT_CONTINUE,
    EVENT_RESULT_STOPPED_BY_USER,
    EVENT_RESULT_STOPPED_BY_EOS,
};

static enum event_result
handle_event(struct scrcpy *s, const struct scrcpy_options *options,
             SDL_Event *event) {
    switch (event->type) {
        case EVENT_STREAM_STOPPED:
            LOGD("Video stream stopped");
            return EVENT_RESULT_STOPPED_BY_EOS;
        case SDL_QUIT:
            LOGD("User requested to quit");
            return EVENT_RESULT_STOPPED_BY_USER;
        case SDL_DROPFILE: {
            if (!options->control) {
                break;
            }
            char *file = strdup(event->drop.file);
            SDL_free(event->drop.file);
            if (!file) {
                LOGW("Could not strdup drop filename\n");
                break;
            }

            file_handler_action_t action;
            if (is_apk(file)) {
                action = ACTION_INSTALL_APK;
            } else {
                action = ACTION_PUSH_FILE;
            }
            file_handler_request(&s->file_handler, action, file);
            goto end;
        }
    }

    bool consumed = screen_handle_event(&s->screen, event);
    if (consumed) {
        goto end;
    }

    consumed = input_manager_handle_event(&s->input_manager, event);
    (void) consumed;

end:
    return EVENT_RESULT_CONTINUE;
}

static bool
event_loop(struct scrcpy *s, const struct scrcpy_options *options) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        enum event_result result = handle_event(s, options, &event);
        switch (result) {
            case EVENT_RESULT_STOPPED_BY_USER:
                return true;
            case EVENT_RESULT_STOPPED_BY_EOS:
                LOGW("Device disconnected");
                return false;
            case EVENT_RESULT_CONTINUE:
                break;
        }
    }
    return false;
}

static bool
await_for_server(void) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                LOGD("User requested to quit");
                return false;
            case EVENT_SERVER_CONNECTION_FAILED:
                LOGE("Server connection failed");
                return false;
            case EVENT_SERVER_CONNECTED:
                LOGD("Server connected");
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
stream_on_eos(struct stream *stream, void *userdata) {
    (void) stream;
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

bool
scrcpy(struct scrcpy_options *options) {
    static struct scrcpy scrcpy;
    struct scrcpy *s = &scrcpy;

    // Minimal SDL initialization
    if (SDL_Init(SDL_INIT_EVENTS)) {
        LOGC("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }

    atexit(SDL_Quit);

    bool ret = false;

    bool server_started = false;
    bool file_handler_initialized = false;
    bool recorder_initialized = false;
#ifdef HAVE_V4L2
    bool v4l2_sink_initialized = false;
#endif
    bool stream_started = false;
#ifdef HAVE_AOA_HID
    bool aoa_hid_initialized = false;
#endif
    bool controller_initialized = false;
    bool controller_started = false;
    bool screen_initialized = false;

    struct sc_acksync *acksync = NULL;

    struct sc_server_params params = {
        .serial = options->serial,
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
        .tcpip = options->tcpip,
        .tcpip_dst = options->tcpip_dst,
    };

    static const struct sc_server_callbacks cbs = {
        .on_connection_failed = sc_server_on_connection_failed,
        .on_connected = sc_server_on_connected,
        .on_disconnected = sc_server_on_disconnected,
    };
    if (!sc_server_init(&s->server, &params, &cbs, NULL)) {
        return false;
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
        LOGC("Could not initialize SDL: %s", SDL_GetError());
        goto end;
    }

    sdl_configure(options->display, options->disable_screensaver);

    // Await for server without blocking Ctrl+C handling
    if (!await_for_server()) {
        goto end;
    }

    // It is necessarily initialized here, since the device is connected
    struct sc_server_info *info = &s->server.info;

    const char *serial = s->server.params.serial;
    assert(serial);

    if (options->display && options->control) {
        if (!file_handler_init(&s->file_handler, serial,
                               options->push_target)) {
            goto end;
        }
        file_handler_initialized = true;
    }

    struct decoder *dec = NULL;
    bool needs_decoder = options->display;
#ifdef HAVE_V4L2
    needs_decoder |= !!options->v4l2_device;
#endif
    if (needs_decoder) {
        decoder_init(&s->decoder);
        dec = &s->decoder;
    }

    struct recorder *rec = NULL;
    if (options->record_filename) {
        if (!recorder_init(&s->recorder,
                           options->record_filename,
                           options->record_format,
                           info->frame_size)) {
            goto end;
        }
        rec = &s->recorder;
        recorder_initialized = true;
    }

    av_log_set_callback(av_log_callback);

    static const struct stream_callbacks stream_cbs = {
        .on_eos = stream_on_eos,
    };
    stream_init(&s->stream, s->server.video_socket, &stream_cbs, NULL);

    if (dec) {
        stream_add_sink(&s->stream, &dec->packet_sink);
    }

    if (rec) {
        stream_add_sink(&s->stream, &rec->packet_sink);
    }

    if (options->control) {
#ifdef HAVE_AOA_HID
        if (options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_HID) {
            bool ok = sc_acksync_init(&s->acksync);
            if (!ok) {
                goto end;
            }

            acksync = &s->acksync;
        }
#endif
        if (!controller_init(&s->controller, s->server.control_socket,
                             acksync)) {
            goto end;
        }
        controller_initialized = true;

        if (!controller_start(&s->controller)) {
            goto end;
        }
        controller_started = true;

        if (options->turn_screen_off) {
            struct control_msg msg;
            msg.type = CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE;
            msg.set_screen_power_mode.mode = SCREEN_POWER_MODE_OFF;

            if (!controller_push_msg(&s->controller, &msg)) {
                LOGW("Could not request 'set screen power mode'");
            }
        }
    }

    if (options->display) {
        const char *window_title =
            options->window_title ? options->window_title : info->device_name;

        struct screen_params screen_params = {
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
            .buffering_time = options->display_buffer,
        };

        if (!screen_init(&s->screen, &screen_params)) {
            goto end;
        }
        screen_initialized = true;

        decoder_add_sink(&s->decoder, &s->screen.frame_sink);
    }

#ifdef HAVE_V4L2
    if (options->v4l2_device) {
        if (!sc_v4l2_sink_init(&s->v4l2_sink, options->v4l2_device,
                               info->frame_size, options->v4l2_buffer)) {
            goto end;
        }

        decoder_add_sink(&s->decoder, &s->v4l2_sink.frame_sink);

        v4l2_sink_initialized = true;
    }
#endif

    // now we consumed the header values, the socket receives the video stream
    // start the stream
    if (!stream_start(&s->stream)) {
        goto end;
    }
    stream_started = true;

    struct sc_key_processor *kp = NULL;
    struct sc_mouse_processor *mp = NULL;

    if (options->control) {
        if (options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_HID) {
#ifdef HAVE_AOA_HID
            bool aoa_hid_ok = false;

            bool ok = sc_aoa_init(&s->aoa, serial, acksync);
            if (!ok) {
                goto aoa_hid_end;
            }

            if (!sc_hid_keyboard_init(&s->keyboard_hid, &s->aoa)) {
                sc_aoa_destroy(&s->aoa);
                goto aoa_hid_end;
            }

            if (!sc_aoa_start(&s->aoa)) {
                sc_hid_keyboard_destroy(&s->keyboard_hid);
                sc_aoa_destroy(&s->aoa);
                goto aoa_hid_end;
            }

            aoa_hid_ok = true;
            kp = &s->keyboard_hid.key_processor;

            aoa_hid_initialized = true;

aoa_hid_end:
            if (!aoa_hid_ok) {
                LOGE("Failed to enable HID over AOA, "
                     "fallback to default keyboard injection method "
                     "(-K/--hid-keyboard ignored)");
                options->keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_INJECT;
            }
#else
            LOGE("HID over AOA is not supported on this platform, "
                 "fallback to default keyboard injection method "
                 "(-K/--hid-keyboard ignored)");
            options->keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_INJECT;
#endif
        }

        // keyboard_input_mode may have been reset if HID mode failed
        if (options->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_INJECT) {
            sc_keyboard_inject_init(&s->keyboard_inject, &s->controller,
                                    options);
            kp = &s->keyboard_inject.key_processor;
        }

        sc_mouse_inject_init(&s->mouse_inject, &s->controller, &s->screen);
        mp = &s->mouse_inject.mouse_processor;
    }

    input_manager_init(&s->input_manager, &s->controller, &s->screen, kp, mp,
                       options);

    ret = event_loop(s, options);
    LOGD("quit...");

    // Close the window immediately on closing, because screen_destroy() may
    // only be called once the stream thread is joined (it may take time)
    screen_hide_window(&s->screen);

end:
    // The stream is not stopped explicitly, because it will stop by itself on
    // end-of-stream
#ifdef HAVE_AOA_HID
    if (aoa_hid_initialized) {
        sc_hid_keyboard_destroy(&s->keyboard_hid);
        sc_aoa_stop(&s->aoa);
    }
    if (acksync) {
        sc_acksync_destroy(acksync);
    }
#endif
    if (controller_started) {
        controller_stop(&s->controller);
    }
    if (file_handler_initialized) {
        file_handler_stop(&s->file_handler);
    }
    if (screen_initialized) {
        screen_interrupt(&s->screen);
    }

    if (server_started) {
        // shutdown the sockets and kill the server
        sc_server_stop(&s->server);
    }

    // now that the sockets are shutdown, the stream and controller are
    // interrupted, we can join them
    if (stream_started) {
        stream_join(&s->stream);
    }

#ifdef HAVE_V4L2
    if (v4l2_sink_initialized) {
        sc_v4l2_sink_destroy(&s->v4l2_sink);
    }
#endif

#ifdef HAVE_AOA_HID
    if (aoa_hid_initialized) {
        sc_aoa_join(&s->aoa);
        sc_aoa_destroy(&s->aoa);
    }
#endif

    // Destroy the screen only after the stream is guaranteed to be finished,
    // because otherwise the screen could receive new frames after destruction
    if (screen_initialized) {
        screen_join(&s->screen);
        screen_destroy(&s->screen);
    }

    if (controller_started) {
        controller_join(&s->controller);
    }
    if (controller_initialized) {
        controller_destroy(&s->controller);
    }

    if (recorder_initialized) {
        recorder_destroy(&s->recorder);
    }

    if (file_handler_initialized) {
        file_handler_join(&s->file_handler);
        file_handler_destroy(&s->file_handler);
    }

    sc_server_destroy(&s->server);

    return ret;
}
