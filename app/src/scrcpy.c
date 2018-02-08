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
#include "decoder.h"
#include "device.h"
#include "events.h"
#include "frames.h"
#include "lockutil.h"
#include "netutil.h"
#include "screen.h"
#include "screencontrol.h"
#include "server.h"
#include "tinyxpm.h"

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
                screencontrol_handle_text_input(&controller, &screen, &event.text);
                break;
            }
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                screencontrol_handle_key(&controller, &screen, &event.key);
                break;
            case SDL_MOUSEMOTION:
                screencontrol_handle_mouse_motion(&controller, &screen, &event.motion);
                break;
            case SDL_MOUSEWHEEL: {
                screencontrol_handle_mouse_wheel(&controller, &screen, &event.wheel);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                screencontrol_handle_mouse_button(&controller, &screen, &event.button);
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
    TCPsocket device_socket = server_connect_to(&server, serial);
    if (!device_socket) {
        server_stop(&server, serial);
        return SDL_FALSE;
    }

    char device_name[DEVICE_NAME_FIELD_LENGTH];
    struct size frame_size;

    // screenrecord does not send frames when the screen content does not change
    // therefore, we transmit the screen size before the video stream, to be able
    // to init the window immediately
    if (!device_read_info(device_socket, device_name, &frame_size)) {
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
