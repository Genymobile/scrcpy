#include "scrcpy.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <sys/time.h>
#include <SDL2/SDL.h>

#include "config.h"
#include "command.h"
#include "common.h"
#include "compat.h"
#include "controller.h"
#include "decoder.h"
#include "device.h"
#include "events.h"
#include "file_handler.h"
#include "fps_counter.h"
#include "input_manager.h"
#include "recorder.h"
#include "screen.h"
#include "server.h"
#include "stream.h"
#include "tiny_xpm.h"
#include "video_buffer.h"
#include "util/lock.h"
#include "util/log.h"
#include "util/net.h"

static struct server server = SERVER_INITIALIZER;
static struct screen screen = SCREEN_INITIALIZER;
static struct fps_counter fps_counter;
static struct video_buffer video_buffer;
static struct stream stream;
static struct decoder decoder;
static struct recorder recorder;
static struct controller controller;
static struct file_handler file_handler;

static struct input_manager input_manager = {
    .controller = &controller,
    .video_buffer = &video_buffer,
    .screen = &screen,
    .prefer_text = false, // initialized later
};

// init SDL and set appropriate hints
static bool
sdl_init_and_configure(bool display) {
    uint32_t flags = display ? SDL_INIT_VIDEO : SDL_INIT_EVENTS;
    if (SDL_Init(flags)) {
        LOGC("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }

    atexit(SDL_Quit);

    if (!display) {
        return true;
    }

    // Use the best available scale quality
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2")) {
        LOGW("Could not enable bilinear filtering");
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

    // Do not disable the screensaver when scrcpy is running
    SDL_EnableScreenSaver();

    return true;
}


#if defined(__APPLE__) || defined(__WINDOWS__)
# define CONTINUOUS_RESIZING_WORKAROUND
#endif

#ifdef CONTINUOUS_RESIZING_WORKAROUND
// On Windows and MacOS, resizing blocks the event loop, so resizing events are
// not triggered. As a workaround, handle them in an event handler.
//
// <https://bugzilla.libsdl.org/show_bug.cgi?id=2077>
// <https://stackoverflow.com/a/40693139/1987178>
static int
event_watcher(void *data, SDL_Event *event) {
    (void) data;
    if (event->type == SDL_WINDOWEVENT
            && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        // called from another thread, not very safe, but it's a workaround!
        screen_render(&screen);
    }
    return 0;
}
#endif

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
handle_event(SDL_Event *event, bool control) {
    switch (event->type) {
        case EVENT_STREAM_STOPPED:
            LOGD("Video stream stopped");
            return EVENT_RESULT_STOPPED_BY_EOS;
        case SDL_QUIT:
            LOGD("User requested to quit");
            return EVENT_RESULT_STOPPED_BY_USER;
        case EVENT_NEW_FRAME:
            if (!screen.has_frame) {
                screen.has_frame = true;
                // this is the very first frame, show the window
                screen_show_window(&screen);
            }
            if (!screen_update_frame(&screen, &video_buffer)) {
                return EVENT_RESULT_CONTINUE;
            }
            break;
        case SDL_WINDOWEVENT:
            screen_handle_window_event(&screen, &event->window);
            break;
        case SDL_TEXTINPUT:
            if (!control) {
                break;
            }
            input_manager_process_text_input(&input_manager, &event->text);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            // some key events do not interact with the device, so process the
            // event even if control is disabled
            input_manager_process_key(&input_manager, &event->key, control);
            break;
        case SDL_MOUSEMOTION:
            if (!control) {
                break;
            }
            input_manager_process_mouse_motion(&input_manager, &event->motion);
            break;
        case SDL_MOUSEWHEEL:
            if (!control) {
                break;
            }
            input_manager_process_mouse_wheel(&input_manager, &event->wheel);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // some mouse events do not interact with the device, so process
            // the event even if control is disabled
            input_manager_process_mouse_button(&input_manager, &event->button,
                                               control);
            break;
        case SDL_FINGERMOTION:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
            input_manager_process_touch(&input_manager, &event->tfinger);
            break;
        case SDL_DROPFILE: {
            if (!control) {
                break;
            }
            file_handler_action_t action;
            if (is_apk(event->drop.file)) {
                action = ACTION_INSTALL_APK;
            } else {
                action = ACTION_PUSH_FILE;
            }
            file_handler_request(&file_handler, action, event->drop.file);
            break;
        }
    }
    return EVENT_RESULT_CONTINUE;
}

static bool
event_loop(bool display, bool control) {
    (void) display;
#ifdef CONTINUOUS_RESIZING_WORKAROUND
    if (display) {
        SDL_AddEventWatch(event_watcher, NULL);
    }
#endif
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        enum event_result result = handle_event(&event, control);
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

static process_t
set_show_touches_enabled(const char *serial, bool enabled) {
    const char *value = enabled ? "1" : "0";
    const char *const adb_cmd[] = {
        "shell", "settings", "put", "system", "show_touches", value
    };
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

static void
wait_show_touches(process_t process) {
    // reap the process, ignore the result
    process_check_success(process, "show_touches");
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
    char *local_fmt = SDL_malloc(strlen(fmt) + 10);
    if (!local_fmt) {
        LOGC("Could not allocate string");
        return;
    }
    // strcpy is safe here, the destination is large enough
    strcpy(local_fmt, "[FFmpeg] ");
    strcpy(local_fmt + 9, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_VIDEO, priority, local_fmt, vl);
    SDL_free(local_fmt);
}

bool
scrcpy(const struct scrcpy_options *options) {
    bool record = !!options->record_filename;
    struct server_params params = {
        .crop = options->crop,
        .local_port = options->port,
        .max_size = options->max_size,
        .bit_rate = options->bit_rate,
        .max_fps = options->max_fps,
        .control = options->control,
    };
    if (!server_start(&server, options->serial, &params)) {
        return false;
    }

    process_t proc_show_touches = PROCESS_NONE;
    bool show_touches_waited;
    if (options->show_touches) {
        LOGI("Enable show_touches");
        proc_show_touches = set_show_touches_enabled(options->serial, true);
        show_touches_waited = false;
    }

    bool ret = false;

    bool fps_counter_initialized = false;
    bool video_buffer_initialized = false;
    bool file_handler_initialized = false;
    bool recorder_initialized = false;
    bool stream_started = false;
    bool controller_initialized = false;
    bool controller_started = false;

    if (!sdl_init_and_configure(options->display)) {
        goto end;
    }

    if (!server_connect_to(&server)) {
        goto end;
    }

    char device_name[DEVICE_NAME_FIELD_LENGTH];
    struct size frame_size;

    // screenrecord does not send frames when the screen content does not
    // change therefore, we transmit the screen size before the video stream,
    // to be able to init the window immediately
    if (!device_read_info(server.video_socket, device_name, &frame_size)) {
        goto end;
    }

    struct decoder *dec = NULL;
    if (options->display) {
        if (!fps_counter_init(&fps_counter)) {
            goto end;
        }
        fps_counter_initialized = true;

        if (!video_buffer_init(&video_buffer, &fps_counter,
                               options->render_expired_frames)) {
            goto end;
        }
        video_buffer_initialized = true;

        if (options->control) {
            if (!file_handler_init(&file_handler, server.serial,
                                   options->push_target)) {
                goto end;
            }
            file_handler_initialized = true;
        }

        decoder_init(&decoder, &video_buffer);
        dec = &decoder;
    }

    struct recorder *rec = NULL;
    if (record) {
        if (!recorder_init(&recorder,
                           options->record_filename,
                           options->record_format,
                           frame_size)) {
            goto end;
        }
        rec = &recorder;
        recorder_initialized = true;
    }

    av_log_set_callback(av_log_callback);

    stream_init(&stream, server.video_socket, dec, rec);

    // now we consumed the header values, the socket receives the video stream
    // start the stream
    if (!stream_start(&stream)) {
        goto end;
    }
    stream_started = true;

    if (options->display) {
        if (options->control) {
            if (!controller_init(&controller, server.control_socket)) {
                goto end;
            }
            controller_initialized = true;

            if (!controller_start(&controller)) {
                goto end;
            }
            controller_started = true;
        }

        const char *window_title =
            options->window_title ? options->window_title : device_name;

        if (!screen_init_rendering(&screen, window_title, frame_size,
                                   options->always_on_top, options->window_x,
                                   options->window_y, options->window_width,
                                   options->window_height,
                                   options->window_borderless)) {
            goto end;
        }

        if (options->turn_screen_off) {
            struct control_msg msg;
            msg.type = CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE;
            msg.set_screen_power_mode.mode = SCREEN_POWER_MODE_OFF;

            if (!controller_push_msg(&controller, &msg)) {
                LOGW("Could not request 'set screen power mode'");
            }
        }

        if (options->fullscreen) {
            screen_switch_fullscreen(&screen);
        }
    }

    if (options->show_touches) {
        wait_show_touches(proc_show_touches);
        show_touches_waited = true;
    }

    input_manager.prefer_text = options->prefer_text;

    ret = event_loop(options->display, options->control);
    LOGD("quit...");

    screen_destroy(&screen);

end:
    // stop stream and controller so that they don't continue once their socket
    // is shutdown
    if (stream_started) {
        stream_stop(&stream);
    }
    if (controller_started) {
        controller_stop(&controller);
    }
    if (file_handler_initialized) {
        file_handler_stop(&file_handler);
    }
    if (fps_counter_initialized) {
        fps_counter_interrupt(&fps_counter);
    }

    // shutdown the sockets and kill the server
    server_stop(&server);

    // now that the sockets are shutdown, the stream and controller are
    // interrupted, we can join them
    if (stream_started) {
        stream_join(&stream);
    }
    if (controller_started) {
        controller_join(&controller);
    }
    if (controller_initialized) {
        controller_destroy(&controller);
    }

    if (recorder_initialized) {
        recorder_destroy(&recorder);
    }

    if (file_handler_initialized) {
        file_handler_join(&file_handler);
        file_handler_destroy(&file_handler);
    }

    if (video_buffer_initialized) {
        video_buffer_destroy(&video_buffer);
    }

    if (fps_counter_initialized) {
        fps_counter_join(&fps_counter);
        fps_counter_destroy(&fps_counter);
    }

    if (options->show_touches) {
        if (!show_touches_waited) {
            // wait the process which enabled "show touches"
            wait_show_touches(proc_show_touches);
        }
        LOGI("Disable show_touches");
        proc_show_touches = set_show_touches_enabled(options->serial, false);
        wait_show_touches(proc_show_touches);
    }

    server_destroy(&server);

    return ret;
}
