#include "screen.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <sys/time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

#include "command.h"
#include "common.h"
#include "control.h"
#include "convert.h"
#include "decoder.h"
#include "events.h"
#include "frames.h"
#include "lockutil.h"
#include "netutil.h"
#include "server.h"

#define DEVICE_NAME_FIELD_LENGTH 64
#define DISPLAY_MARGINS 96

static struct frames frames;
static struct decoder decoder;
static struct controller controller;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static struct size frame_size;
static SDL_bool texture_empty = SDL_TRUE;
static SDL_bool fullscreen = SDL_FALSE;

static long timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void count_frame(void) {
    static long ts = 0;
    static int nbframes = 0;
    long now = timestamp_ms();
    ++nbframes;
    if (now - ts > 1000) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%d fps", nbframes);
        ts = now;
        nbframes = 0;
    }
}

static TCPsocket listen_on_port(Uint16 port) {
    IPaddress addr = {
        .host = INADDR_ANY,
        .port = SDL_SwapBE16(port),
    };
    return SDLNet_TCP_Open(&addr);
}

// name must be at least DEVICE_NAME_FIELD_LENGTH bytes
SDL_bool read_initial_device_info(TCPsocket socket, char *device_name, struct size *size) {
    unsigned char buf[DEVICE_NAME_FIELD_LENGTH + 4];
    if (SDLNet_TCP_Recv(socket, buf, sizeof(buf)) <= 0) {
        return SDL_FALSE;
    }
    buf[DEVICE_NAME_FIELD_LENGTH - 1] = '\0'; // in case the client sends garbage
    // scrcpy is safe here, since name contains at least DEVICE_NAME_FIELD_LENGTH bytes
    // and strlen(buf) < DEVICE_NAME_FIELD_LENGTH
    strcpy(device_name, (char *) buf);
    size->width = (buf[DEVICE_NAME_FIELD_LENGTH] << 8) | buf[DEVICE_NAME_FIELD_LENGTH + 1];
    size->height = (buf[DEVICE_NAME_FIELD_LENGTH + 2] << 8) | buf[DEVICE_NAME_FIELD_LENGTH + 3];
    return SDL_TRUE;
}

#if SDL_VERSION_ATLEAST(2, 0, 5)
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayUsableBounds((i), (r))
#else
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayBounds((i), (r))
#endif

// init the preferred display_bounds (i.e. the screen bounds with some margins)
static SDL_bool get_preferred_display_bounds(struct size *bounds) {
    SDL_Rect rect;
    if (GET_DISPLAY_BOUNDS(0, &rect)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Could not get display usable bounds: %s", SDL_GetError());
        return SDL_FALSE;
    }

    bounds->width = MAX(0, rect.w - DISPLAY_MARGINS);
    bounds->height = MAX(0, rect.h - DISPLAY_MARGINS);
    return SDL_TRUE;
}

static inline struct size get_window_size(SDL_Window *window) {
    int width;
    int height;
    SDL_GetWindowSize(window, &width, &height);

    struct size size;
    size.width = width;
    size.height = height;
    return size;
}

static inline struct position get_mouse_position() {
    int x;
    int y;
    SDL_GetMouseState(&x, &y);
    SDL_assert_release(x >= 0 && x < 0x10000 && y >= 0 && y < 0x10000);
    return (struct position) {
        .x = (Uint16) x,
        .y = (Uint16) y,
    };
}

// return the optimal size of the window, with the following constraints:
//  - it attempts to keep at least one dimension of the current_size (i.e. it crops the black borders)
//  - it keeps the aspect ratio
//  - it scales down to make it fit in the display_size
// TODO unit test
static struct size get_optimal_size(struct size current_size, struct size frame_size) {
    struct size display_size;
    // 32 bits because we need to multiply two 16 bits values
    Uint32 w;
    Uint32 h;

    if (!get_preferred_display_bounds(&display_size)) {
        // cannot get display bounds, do not constraint the size
        w = current_size.width;
        h = current_size.height;
    } else {
        w = MIN(current_size.width, display_size.width);
        h = MIN(current_size.height, display_size.height);
    }

    SDL_bool keep_width = frame_size.width * h > frame_size.height * w;
    if (keep_width) {
        // remove black borders on top and bottom
        h = frame_size.height * w / frame_size.width;
    } else {
        // remove black borders on left and right (or none at all if it already fits)
        w = frame_size.width * h / frame_size.height;
    }

    // w and h must fit into 16 bits
    SDL_assert_release(w < 0x10000 && h < 0x10000);
    return (struct size) {w, h};
}

// initially, there is no current size, so use the frame size as current size
static inline struct size get_initial_optimal_size(struct size frame_size) {
    return get_optimal_size(frame_size, frame_size);
}

// same as get_optimal_size(), but read the current size from the window
static inline struct size get_optimal_window_size(SDL_Window *window, struct size frame_size) {
    struct size current_size = get_window_size(window);
    return get_optimal_size(current_size, frame_size);
}

static inline SDL_bool prepare_for_frame(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture **texture,
                                         struct size old_frame_size, struct size frame_size) {
    (void) window; // might be used to resize the window automatically
    if (old_frame_size.width != frame_size.width || old_frame_size.height != frame_size.height) {
        if (SDL_RenderSetLogicalSize(renderer, frame_size.width, frame_size.height)) {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Could not set renderer logical size: %s", SDL_GetError());
            return SDL_FALSE;
        }

        // frame dimension changed, destroy texture
        SDL_DestroyTexture(*texture);

        struct size current_size = get_window_size(window);
        struct size target_size = {
            (Uint32) current_size.width * frame_size.width / old_frame_size.width,
            (Uint32) current_size.height * frame_size.height / old_frame_size.height,
        };
        target_size = get_optimal_size(target_size, frame_size);
        SDL_SetWindowSize(window, target_size.width, target_size.height);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "New texture: %" PRIu16 "x%" PRIu16, frame_size.width, frame_size.height);
        *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, frame_size.width, frame_size.height);
        if (!*texture) {
            SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Could not create texture: %s", SDL_GetError());
            return SDL_FALSE;
        }
    }

    return SDL_TRUE;
}

static void update_texture(const AVFrame *frame, SDL_Texture *texture) {
    SDL_UpdateYUVTexture(texture, NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);
}

static void render(SDL_Renderer *renderer, SDL_Texture *texture) {
    SDL_RenderClear(renderer);
    if (texture) {
        SDL_RenderCopy(renderer, texture, NULL, NULL);
    }
    SDL_RenderPresent(renderer);
}

static int wait_for_success(process_t proc, const char *name) {
    if (proc == PROCESS_NONE) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Could not execute \"%s\"", name);
        return -1;
    }
    exit_code_t exit_code;
    if (!cmd_simple_wait(proc, &exit_code)) {
        if (exit_code != NO_EXIT_CODE) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "\"%s\" returned with value %" PRIexitcode, name, exit_code);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "\"%s\" exited unexpectedly", name);
        }
        return -1;
    }
    return 0;
}

static SDL_bool handle_new_frame(void) {
    mutex_lock(frames.mutex);
    AVFrame *frame = frames.rendering_frame;
    frames.rendering_frame_consumed = SDL_TRUE;
    if (!decoder.skip_frames) {
        cond_signal(frames.rendering_frame_consumed_cond);
    }

    struct size current_frame_size = {frame->width, frame->height};
    if (!prepare_for_frame(window, renderer, &texture, frame_size, current_frame_size)) {
        return SDL_FALSE;
    }

    frame_size = current_frame_size;

    update_texture(frame, texture);
    mutex_unlock(frames.mutex);

    render(renderer, texture);
    return SDL_TRUE;
}

static void handle_text_input(const SDL_TextInputEvent *event) {
    struct control_event control_event;
    control_event.type = CONTROL_EVENT_TYPE_TEXT;
    strncpy(control_event.text_event.text, event->text, TEXT_MAX_LENGTH);
    control_event.text_event.text[TEXT_MAX_LENGTH] = '\0';
    if (!controller_push_event(&controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send text event");
    }
}

static void handle_key(const SDL_KeyboardEvent *event) {
    SDL_Keycode keycode = event->keysym.sym;
    SDL_bool ctrl = event->keysym.mod & (KMOD_LCTRL | KMOD_RCTRL);
    SDL_bool shift = event->keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT);
    SDL_bool repeat = event->repeat;

    // Capture Ctrl+x: optimal size
    if (keycode == SDLK_x && !repeat && ctrl && !shift) {
        if (event->type == SDL_KEYDOWN) {
            struct size optimal_size = get_optimal_window_size(window, frame_size);
            SDL_SetWindowSize(window, optimal_size.width, optimal_size.height);
        }
        return;
    }

    // Capture Ctrl+f: switch fullscreen
    if (keycode == SDLK_f && !repeat && ctrl && !shift) {
        if (event->type == SDL_KEYDOWN) {
            Uint32 new_mode = fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
            if (!SDL_SetWindowFullscreen(window, new_mode)) {
                fullscreen = !fullscreen;
                render(renderer, texture_empty ? NULL : texture);
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not switch fullscreen mode: %s", SDL_GetError());
            }
        }
        return;
    }

    struct control_event control_event;
    if (input_key_from_sdl_to_android(event, &control_event)) {
        if (!controller_push_event(&controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send control event");
        }
    }
}

static void handle_mouse_motion(const SDL_MouseMotionEvent *event, struct size screen_size) {
    struct control_event control_event;
    if (mouse_motion_from_sdl_to_android(event, screen_size, &control_event)) {
        if (!controller_push_event(&controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send mouse motion event");
        }
    }
}

static void handle_mouse_button(const SDL_MouseButtonEvent *event, struct size screen_size) {
    struct control_event control_event;
    if (mouse_button_from_sdl_to_android(event, screen_size, &control_event)) {
        if (!controller_push_event(&controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send mouse button event");
        }
    }
}

static void handle_mouse_wheel(const SDL_MouseWheelEvent *event, struct point point) {
    struct control_event control_event;
    if (mouse_wheel_from_sdl_to_android(event, point, &control_event)) {
        if (!controller_push_event(&controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send wheel button event");
        }
    }
}

void event_loop(void) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
        case EVENT_DECODER_STOPPED:
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Video decoder stopped");
        case SDL_QUIT:
            return;
        case EVENT_NEW_FRAME:
            if (!handle_new_frame()) {
                return;
            }
            texture_empty = SDL_FALSE;
            count_frame(); // display fps for debug
            break;
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_EXPOSED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                render(renderer, texture_empty ? NULL : texture);
                break;
            }
            break;
        case SDL_TEXTINPUT: {
            handle_text_input(&event.text);
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            handle_key(&event.key);
            break;
        case SDL_MOUSEMOTION:
            handle_mouse_motion(&event.motion, frame_size);
            break;
        case SDL_MOUSEWHEEL: {
            struct point point = {
                .screen_size = frame_size,
                .position = get_mouse_position(),
            };
            handle_mouse_wheel(&event.wheel, point);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            handle_mouse_button(&event.button, frame_size);
            break;
        }
        }
    }
}

SDL_bool show_screen(const char *serial, Uint16 local_port) {
    SDL_bool ret = 0;

    process_t push_proc = push_server(serial);
    if (wait_for_success(push_proc, "adb push")) {
        return SDL_FALSE;
    }

    process_t reverse_tunnel_proc = enable_tunnel(serial, local_port);
    if (wait_for_success(reverse_tunnel_proc, "adb reverse")) {
        return SDL_FALSE;
    }

    TCPsocket server_socket = listen_on_port(local_port);
    if (!server_socket) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not open video socket");
        goto screen_finally_adb_reverse_remove;
    }

    // server will connect to our socket
    process_t server = start_server(serial);
    if (server == PROCESS_NONE) {
        ret = SDL_FALSE;
        SDLNet_TCP_Close(server_socket);
        goto screen_finally_adb_reverse_remove;
    }

    // to reduce startup time, we could be tempted to init other stuff before blocking here
    // but we should not block after SDL_Init since it handles the signals (Ctrl+C) in its
    // event loop: blocking could lead to deadlock
    TCPsocket device_socket = blocking_accept(server_socket);
    // we don't need the server socket anymore
    SDLNet_TCP_Close(server_socket);
    if (!device_socket) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not accept video socket: %s", SDL_GetError());
        ret = SDL_FALSE;
        stop_server(server);
        goto screen_finally_adb_reverse_remove;
    }

    char device_name[DEVICE_NAME_FIELD_LENGTH];

    // screenrecord does not send frames when the screen content does not change
    // therefore, we transmit the screen size before the video stream, to be able
    // to init the window immediately
    if (!read_initial_device_info(device_socket, device_name, &frame_size)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not retrieve initial screen size");
        ret = SDL_FALSE;
        SDLNet_TCP_Close(device_socket);
        stop_server(server);
        goto screen_finally_adb_reverse_remove;
    }

    if (!frames_init(&frames)) {
        ret = SDL_FALSE;
        SDLNet_TCP_Close(device_socket);
        stop_server(server);
        goto screen_finally_adb_reverse_remove;
    }

    decoder.frames = &frames;
    decoder.video_socket = device_socket;
    decoder.skip_frames = SDL_TRUE;

    // now we consumed the header values, the socket receives the video stream
    // start the decoder
    if (!decoder_start(&decoder)) {
        ret = SDL_FALSE;
        SDLNet_TCP_Close(device_socket);
        stop_server(server);
        goto screen_finally_destroy_frames;
    }

    if (!controller_init(&controller, device_socket)) {
        ret = SDL_FALSE;
        SDLNet_TCP_Close(device_socket);
        stop_server(server);
        goto screen_finally_stop_decoder;
    }

    if (!controller_start(&controller)) {
        ret = SDL_FALSE;
        SDLNet_TCP_Close(device_socket);
        stop_server(server);
        goto screen_finally_destroy_controller;
    }

    if (SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL: %s", SDL_GetError());
        ret = SDL_FALSE;
        goto screen_finally_stop_and_join_controller;
    }
    // FIXME it may crash in SDL_Quit in i965_dri.so
    // As a workaround, do not call SDL_Quit() (we are exiting anyway).
    // atexit(SDL_Quit);

    // Bilinear resizing
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Could not enable bilinear filtering");
    }

    struct size window_size = get_initial_optimal_size(frame_size);
    window = SDL_CreateWindow(device_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          window_size.width, window_size.height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not create window: %s", SDL_GetError());
        ret = SDL_FALSE;
        goto screen_finally_stop_decoder;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Could not create renderer: %s", SDL_GetError());
        ret = SDL_FALSE;
        goto screen_finally_destroy_window;
    }

    if (SDL_RenderSetLogicalSize(renderer, frame_size.width, frame_size.height)) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Could not set renderer logical size: %s", SDL_GetError());
        ret = SDL_FALSE;
        goto screen_finally_destroy_renderer;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Initial texture: %" PRIu16 "x%" PRIu16, frame_size.width, frame_size.height);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, frame_size.width, frame_size.height);
    if (!texture) {
        SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Could not create texture: %s", SDL_GetError());
        ret = SDL_FALSE;
        goto screen_finally_destroy_renderer;
    }

    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    event_loop();

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "quit...");
    SDL_DestroyTexture(texture);
screen_finally_destroy_renderer:
    // FIXME it may crash at exit if we destroy the renderer or the window,
    // with the exact same stack trace as <https://bugs.launchpad.net/mir/+bug/1466535>.
    // As a workaround, leak the renderer and the window (we are exiting anyway).
    //SDL_DestroyRenderer(renderer);
screen_finally_destroy_window:
    //SDL_DestroyWindow(window);
screen_finally_stop_and_join_controller:
    controller_stop(&controller);
    controller_join(&controller);
screen_finally_destroy_controller:
    controller_destroy(&controller);
screen_finally_stop_decoder:
    SDLNet_TCP_Close(device_socket);
    // kill the server before decoder_join() to wake up the decoder
    stop_server(server);
    decoder_join(&decoder);
screen_finally_destroy_frames:
    frames_destroy(&frames);
screen_finally_adb_reverse_remove:
    {
        process_t remove = disable_tunnel(serial);
        if (remove != PROCESS_NONE) {
            // ignore failure
            cmd_simple_wait(remove, NULL);
        }
    }

    return ret;
}
