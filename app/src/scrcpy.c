#include "scrcpy.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <sys/time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

#include "command.h"
#include "common.h"
#include "controller.h"
#include "convert.h"
#include "decoder.h"
#include "events.h"
#include "frames.h"
#include "lockutil.h"
#include "netutil.h"
#include "screen.h"
#include "server.h"
#include "tinyxpm.h"

#define DEVICE_NAME_FIELD_LENGTH 64

static struct server server = SERVER_INITIALIZER;
static struct screen screen = SCREEN_INITIALIZER;
static struct frames frames;
static struct decoder decoder;
static struct controller controller;

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

// name must be at least DEVICE_NAME_FIELD_LENGTH bytes
static SDL_bool read_initial_device_info(TCPsocket socket, char *device_name, struct size *size) {
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

static struct point get_mouse_point(void) {
    int x;
    int y;
    SDL_GetMouseState(&x, &y);
    SDL_assert_release(x >= 0 && x < 0x10000 && y >= 0 && y < 0x10000);
    return (struct point) {
        .x = (Uint16) x,
        .y = (Uint16) y,
    };
}

static void send_keycode(enum android_keycode keycode, const char *name) {
    // send DOWN event
    struct control_event control_event = {
        .type = CONTROL_EVENT_TYPE_KEYCODE,
        .keycode_event = {
            .action = AKEY_EVENT_ACTION_DOWN,
            .keycode = keycode,
            .metastate = 0,
        },
    };

    if (!controller_push_event(&controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send %s (DOWN)", name);
        return;
    }

    // send UP event
    control_event.keycode_event.action = AKEY_EVENT_ACTION_UP;
    if (!controller_push_event(&controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send %s (UP)", name);
    }
}

static inline void action_home(void) {
    send_keycode(AKEYCODE_HOME, "HOME");
}

static inline void action_back(void) {
    send_keycode(AKEYCODE_BACK, "BACK");
}

static inline void action_app_switch(void) {
    send_keycode(AKEYCODE_APP_SWITCH, "APP_SWITCH");
}

static inline void action_power(void) {
    send_keycode(AKEYCODE_POWER, "POWER");
}

static inline void action_volume_up(void) {
    send_keycode(AKEYCODE_VOLUME_UP, "VOLUME_UP");
}

static inline void action_volume_down(void) {
    send_keycode(AKEYCODE_VOLUME_DOWN, "VOLUME_DOWN");
}

static void turn_screen_on(void) {
    struct control_event control_event = {
        .type = CONTROL_EVENT_TYPE_COMMAND,
        .command_event = {
            .action = CONTROL_EVENT_COMMAND_SCREEN_ON,
        },
    };
    if (!controller_push_event(&controller, &control_event)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot turn screen on");
    }
}

static SDL_bool handle_new_frame(void) {
    mutex_lock(frames.mutex);
    AVFrame *frame = frames.rendering_frame;
    frames.rendering_frame_consumed = SDL_TRUE;
#ifndef SKIP_FRAMES
    // if SKIP_FRAMES is disabled, then notify the decoder the current frame is
    // consumed, so that it may push a new one
    cond_signal(frames.rendering_frame_consumed_cond);
#endif

    if (!screen_update(&screen, frame)){
        return SDL_FALSE;
    }
    mutex_unlock(frames.mutex);

    screen_render(&screen);
    return SDL_TRUE;
}

static SDL_bool is_ctrl_down(void) {
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
}

static void handle_text_input(const SDL_TextInputEvent *event) {
    if (is_ctrl_down()) {
        switch (event->text[0]) {
            case '+':
                action_volume_up();
                break;
            case '-':
                action_volume_down();
                break;
        }
        return;
    }

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

    // capture all Ctrl events
    if (ctrl) {
        // only consider keydown events, and ignore repeated events
        if (repeat || event->type != SDL_KEYDOWN) {
            return;
        }

        if (shift) {
            // currently, there is no shortcut implying SHIFT
            return;
        }

        switch (keycode) {
            case SDLK_h:
                action_home();
                return;
            case SDLK_b: // fall-through
            case SDLK_BACKSPACE:
                action_back();
                return;
            case SDLK_m:
                action_app_switch();
                return;
            case SDLK_p:
                action_power();
                return;
            case SDLK_f:
                screen_switch_fullscreen(&screen);
                return;
            case SDLK_x:
                screen_resize_to_fit(&screen);
                return;
            case SDLK_g:
                screen_resize_to_pixel_perfect(&screen);
                return;
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
    if (!event->state) {
        // do not send motion events when no button is pressed
        return;
    }
    struct control_event control_event;
    if (mouse_motion_from_sdl_to_android(event, screen_size, &control_event)) {
        if (!controller_push_event(&controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send mouse motion event");
        }
    }
}

static void handle_mouse_button(const SDL_MouseButtonEvent *event, struct size screen_size) {
    if (event->button == SDL_BUTTON_RIGHT) {
        turn_screen_on();
        return;
    };
    struct control_event control_event;
    if (mouse_button_from_sdl_to_android(event, screen_size, &control_event)) {
        if (!controller_push_event(&controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send mouse button event");
        }
    }
}

static void handle_mouse_wheel(const SDL_MouseWheelEvent *event, struct position position) {
    struct control_event control_event;
    if (mouse_wheel_from_sdl_to_android(event, position, &control_event)) {
        if (!controller_push_event(&controller, &control_event)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot send wheel button event");
        }
    }
}

static void event_loop(void) {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case EVENT_DECODER_STOPPED:
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Video decoder stopped");
                return;
            case SDL_QUIT:
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "User requested to quit");
                return;
            case EVENT_NEW_FRAME:
                if (!handle_new_frame()) {
                    return;
                }
                count_frame(); // display fps for debug
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                case SDL_WINDOWEVENT_EXPOSED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    screen_render(&screen);
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
                handle_mouse_motion(&event.motion, screen.frame_size);
                break;
            case SDL_MOUSEWHEEL: {
                struct position position = {
                    .screen_size = screen.frame_size,
                    .point = get_mouse_point(),
                };
                handle_mouse_wheel(&event.wheel, position);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                handle_mouse_button(&event.button, screen.frame_size);
                break;
            }
        }
    }
}

SDL_bool scrcpy(const char *serial, Uint16 local_port, Uint16 max_size, Uint32 bit_rate) {
    if (!server_start(&server, serial, local_port, max_size, bit_rate)) {
        return SDL_FALSE;
    }

    // to reduce startup time, we could be tempted to init other stuff before blocking here
    // but we should not block after SDL_Init since it handles the signals (Ctrl+C) in its
    // event loop: blocking could lead to deadlock
    TCPsocket device_socket = server_connect_to(&server);
    if (!device_socket) {
        server_stop(&server, serial);
        return SDL_FALSE;
    }

    char device_name[DEVICE_NAME_FIELD_LENGTH];
    struct size frame_size;

    // screenrecord does not send frames when the screen content does not change
    // therefore, we transmit the screen size before the video stream, to be able
    // to init the window immediately
    if (!read_initial_device_info(device_socket, device_name, &frame_size)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not retrieve initial screen size");
        server_stop(&server, serial);
        return SDL_FALSE;
    }

    if (!frames_init(&frames)) {
        server_stop(&server, serial);
        return SDL_FALSE;
    }

    SDL_bool ret = SDL_TRUE;

    decoder.frames = &frames;
    decoder.video_socket = device_socket;

    // now we consumed the header values, the socket receives the video stream
    // start the decoder
    if (!decoder_start(&decoder)) {
        ret = SDL_FALSE;
        server_stop(&server, serial);
        goto finally_destroy_frames;
    }

    if (!controller_init(&controller, device_socket)) {
        ret = SDL_FALSE;
        goto finally_stop_decoder;
    }

    if (!controller_start(&controller)) {
        ret = SDL_FALSE;
        goto finally_destroy_controller;
    }

    if (!sdl_init_and_configure()) {
        ret = SDL_FALSE;
        goto finally_stop_and_join_controller;
    }

    if (!screen_init_rendering(&screen, device_name, frame_size)) {
        ret = SDL_FALSE;
        goto finally_stop_and_join_controller;
    }

    event_loop();

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "quit...");
    screen_destroy(&screen);
finally_stop_and_join_controller:
    controller_stop(&controller);
    controller_join(&controller);
finally_destroy_controller:
    controller_destroy(&controller);
finally_stop_decoder:
    // kill the server before decoder_join() to wake up the decoder
    server_stop(&server, serial);
    decoder_join(&decoder);
finally_destroy_frames:
    frames_destroy(&frames);

    return ret;
}
