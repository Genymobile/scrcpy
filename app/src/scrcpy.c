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
#include "recorder.h"
#include "screen.h"
#include "server.h"
#include "stream.h"
#include "tiny_xpm.h"
#include "util/log.h"
#include "util/net.h"
#ifdef HAVE_V4L2
# include "v4l2_sink.h"
#endif

struct scrcpy {
    struct server server;
    struct screen screen;
    struct stream stream;
    struct decoder decoder;
    struct recorder recorder;
#ifdef HAVE_V4L2
    struct sc_v4l2_sink v4l2_sink;
#endif
    struct controller controller;
    struct file_handler file_handler;
    struct input_manager input_manager;
};

#ifdef _WIN32
BOOL WINAPI windows_ctrl_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT) {
        SDL_Event event;
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
        return TRUE;
    }
    return FALSE;
}
#endif // _WIN32

// init SDL and set appropriate hints
static bool
sdl_init_and_configure(bool display, const char *render_driver,
                       bool disable_screensaver) {
    uint32_t flags = display ? SDL_INIT_VIDEO : SDL_INIT_EVENTS;
    if (SDL_Init(flags)) {
        LOGC("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }

    atexit(SDL_Quit);

#ifdef _WIN32
    // Clean up properly on Ctrl+C on Windows
    bool ok = SetConsoleCtrlHandler(windows_ctrl_handler, TRUE);
    if (!ok) {
        LOGW("Could not set Ctrl+C handler");
    }
#endif // _WIN32

    if (!display) {
        return true;
    }

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

    if (disable_screensaver) {
        LOGD("Screensaver disabled");
        SDL_DisableScreenSaver();
    } else {
        LOGD("Screensaver enabled");
        SDL_EnableScreenSaver();
    }

    return true;
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
        LOGC("Could not allocate string");
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

    SDL_Event stop_event;
    stop_event.type = EVENT_STREAM_STOPPED;
    SDL_PushEvent(&stop_event);
}

bool
scrcpy(const struct scrcpy_options *options) {
    static struct scrcpy scrcpy;
    struct scrcpy *s = &scrcpy;

    if (!server_init(&s->server)) {
        return false;
    }

    bool ret = false;

    bool server_started = false;
    bool file_handler_initialized = false;
    bool recorder_initialized = false;
#ifdef HAVE_V4L2
    bool v4l2_sink_initialized = false;
#endif
    bool stream_started = false;
    bool controller_initialized = false;
    bool controller_started = false;
    bool screen_initialized = false;

    bool record = !!options->record_filename;
    struct server_params params = {
        .serial = options->serial,
        .log_level = options->log_level,
        .crop = options->crop,
        .port_range = options->port_range,
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
    };
    if (!server_start(&s->server, &params)) {
        goto end;
    }

    server_started = true;

    if (!sdl_init_and_configure(options->display, options->render_driver,
                                options->disable_screensaver)) {
        goto end;
    }

    char device_name[DEVICE_NAME_FIELD_LENGTH];
    struct size frame_size;

    if (!server_connect_to(&s->server, device_name, &frame_size)) {
        goto end;
    }

    if (options->display && options->control) {
        if (!file_handler_init(&s->file_handler, s->server.serial,
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
    if (record) {
        if (!recorder_init(&s->recorder,
                           options->record_filename,
                           options->record_format,
                           frame_size)) {
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
        if (!controller_init(&s->controller, s->server.control_socket)) {
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
            options->window_title ? options->window_title : device_name;

        struct screen_params screen_params = {
            .window_title = window_title,
            .frame_size = frame_size,
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
        if (!sc_v4l2_sink_init(&s->v4l2_sink, options->v4l2_device, frame_size,
                               options->v4l2_buffer)) {
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

    input_manager_init(&s->input_manager, &s->controller, &s->screen, options);

    ret = event_loop(s, options);
    LOGD("quit...");

    // Close the window immediately on closing, because screen_destroy() may
    // only be called once the stream thread is joined (it may take time)
    screen_hide_window(&s->screen);

end:
    // The stream is not stopped explicitly, because it will stop by itself on
    // end-of-stream
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
        server_stop(&s->server);
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

    server_destroy(&s->server);

    return ret;
}
