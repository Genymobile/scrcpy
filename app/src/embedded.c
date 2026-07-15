#include "embedded.h"

#include <assert.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "events.h"
#include "options.h"
#include "sdl_hints.h"
#include "util/log.h"
#include "util/net.h"
#include "util/sdl.h"

enum sc_embedded_phase {
    SC_EMBEDDED_PHASE_IDLE,
    SC_EMBEDDED_PHASE_WAIT_SERVER,
    SC_EMBEDDED_PHASE_SESSION,
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_t session_thread;
static bool session_thread_joinable;
static bool initialized;
static bool stop_requested;
static bool server_result_ready;
static bool server_connected;
static bool session_result_ready;
static enum scrcpy_exit_code session_result;
static enum sc_embedded_phase phase = SC_EMBEDDED_PHASE_IDLE;
static enum scrcpy_embedded_status status = SCRCPY_EMBEDDED_IDLE;
static struct sc_screen *active_screen;

bool
sc_embedded_is_enabled(void) {
    return initialized;
}

static void *
run_session(void *userdata) {
    char *serial = userdata;

    struct scrcpy_options options = scrcpy_options_default;
    options.serial = serial;
    options.audio_source = SC_AUDIO_SOURCE_OUTPUT;
    // AAC is mandatory in Android's media stack and is also available on
    // emulator/cloud devices that commonly omit the optional Opus encoder.
    options.audio_codec = SC_CODEC_AAC;
    options.keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_SDK;
    options.mouse_input_mode = SC_MOUSE_INPUT_MODE_SDK;
    options.gamepad_input_mode = SC_GAMEPAD_INPUT_MODE_DISABLED;
    options.window_aspect_ratio_lock = false;
    options.render_fit = SC_RENDER_FIT_LETTERBOX;
    options.update_terminal_title = false;

    enum scrcpy_exit_code result = scrcpy(&options);
    free(serial);

    pthread_mutex_lock(&mutex);
    active_screen = NULL;
    phase = SC_EMBEDDED_PHASE_IDLE;
    if (result == SCRCPY_EXIT_DISCONNECTED) {
        status = SCRCPY_EMBEDDED_DISCONNECTED;
    } else if (result == SCRCPY_EXIT_FAILURE) {
        status = SCRCPY_EMBEDDED_FAILED;
    } else {
        status = SCRCPY_EMBEDDED_IDLE;
    }
    pthread_mutex_unlock(&mutex);

    sc_main_thread_destroy();
    return NULL;
}

bool
scrcpy_embedded_initialize(void *nswindow, void *nsview,
                           const char *adb_path, const char *server_path,
                           const char *icon_dir) {
    assert(SDL_IsMainThread());
    if (!nswindow || !nsview || !adb_path || !server_path) {
        return false;
    }

    if (initialized) {
        pthread_mutex_lock(&mutex);
        bool idle = phase == SC_EMBEDDED_PHASE_IDLE;
        pthread_mutex_unlock(&mutex);
        if (!idle) {
            return false;
        }
        sc_sdl_set_embedded_host(nswindow, nsview);
        return true;
    }

    if (setenv("ADB", adb_path, 1) ||
            setenv("SCRCPY_SERVER_PATH", server_path, 1)) {
        return false;
    }
    if (icon_dir && setenv("SCRCPY_ICON_DIR", icon_dir, 1)) {
        return false;
    }

    sc_sdl_set_embedded_host(nswindow, nsview);
    sc_sdl_set_hints(NULL, false);

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        LOGE("Could not initialize embedded SDL: %s", SDL_GetError());
        return false;
    }
    if (!net_init()) {
        return false;
    }

    initialized = true;
    return true;
}

bool
scrcpy_embedded_start(const char *serial) {
    if (!initialized || !serial || !*serial) {
        return false;
    }

    pthread_mutex_lock(&mutex);
    bool busy = phase != SC_EMBEDDED_PHASE_IDLE
             || status == SCRCPY_EMBEDDED_STARTING
             || status == SCRCPY_EMBEDDED_RUNNING;
    pthread_mutex_unlock(&mutex);
    if (busy) {
        return false;
    }

    if (session_thread_joinable) {
        pthread_join(session_thread, NULL);
        session_thread_joinable = false;
    }

    char *serial_copy = strdup(serial);
    if (!serial_copy) {
        return false;
    }
    if (!sc_main_thread_init()) {
        free(serial_copy);
        return false;
    }

    pthread_mutex_lock(&mutex);
    stop_requested = false;
    server_result_ready = false;
    server_connected = false;
    session_result_ready = false;
    session_result = SCRCPY_EXIT_FAILURE;
    active_screen = NULL;
    phase = SC_EMBEDDED_PHASE_WAIT_SERVER;
    status = SCRCPY_EMBEDDED_STARTING;
    pthread_mutex_unlock(&mutex);

    int rc = pthread_create(&session_thread, NULL, run_session, serial_copy);
    if (rc) {
        pthread_mutex_lock(&mutex);
        phase = SC_EMBEDDED_PHASE_IDLE;
        status = SCRCPY_EMBEDDED_FAILED;
        pthread_mutex_unlock(&mutex);
        sc_main_thread_destroy();
        free(serial_copy);
        return false;
    }

    session_thread_joinable = true;
    return true;
}

void
scrcpy_embedded_stop(void) {
    pthread_mutex_lock(&mutex);
    stop_requested = true;
    pthread_mutex_unlock(&mutex);

    sc_push_event(SDL_EVENT_QUIT);
}

enum scrcpy_embedded_status
scrcpy_embedded_get_status(void) {
    pthread_mutex_lock(&mutex);
    enum scrcpy_embedded_status current = status;
    pthread_mutex_unlock(&mutex);
    return current;
}

static struct sc_screen *
get_active_screen(void) {
    pthread_mutex_lock(&mutex);
    struct sc_screen *screen = status == SCRCPY_EMBEDDED_RUNNING
                             ? active_screen
                             : NULL;
    pthread_mutex_unlock(&mutex);
    return screen;
}

static bool
dispatch_input_event(SDL_Event *event) {
    assert(SDL_IsMainThread());
    struct sc_screen *screen = get_active_screen();
    if (!screen || screen->disconnected) {
        return false;
    }

    event->button.windowID = SDL_GetWindowID(screen->window);
    sc_screen_handle_event(screen, event);
    return true;
}

bool
scrcpy_embedded_mouse_motion(float x, float y, float xrel, float yrel,
                             uint32_t buttons) {
    SDL_Event event = {0};
    event.motion.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.state = buttons;
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    return dispatch_input_event(&event);
}

bool
scrcpy_embedded_mouse_button(bool down, uint8_t button, float x, float y,
                             uint8_t clicks) {
    SDL_Event event = {0};
    event.button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN
                             : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = clicks;
    event.button.x = x;
    event.button.y = y;
    return dispatch_input_event(&event);
}

bool
scrcpy_embedded_mouse_wheel(float scroll_x, float scroll_y,
                            float mouse_x, float mouse_y) {
    SDL_Event event = {0};
    event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.x = scroll_x;
    event.wheel.y = scroll_y;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouse_x = mouse_x;
    event.wheel.mouse_y = mouse_y;
    return dispatch_input_event(&event);
}

bool
scrcpy_embedded_key(bool down, const char *key_name,
                    bool shift, bool control, bool alt, bool command,
                    bool repeat) {
    assert(SDL_IsMainThread());
    if (!key_name || !*key_name) {
        return false;
    }

    SDL_Keycode key = SDL_GetKeyFromName(key_name);
    if (key == SDLK_UNKNOWN) {
        return false;
    }

    SDL_Keymod mod = SDL_KMOD_NONE;
    if (shift) {
        mod |= SDL_KMOD_LSHIFT;
    }
    if (control) {
        mod |= SDL_KMOD_LCTRL;
    }
    if (alt) {
        mod |= SDL_KMOD_LALT;
    }
    if (command) {
        mod |= SDL_KMOD_LGUI;
    }
    SDL_SetModState(mod);

    SDL_Event event = {0};
    event.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = SDL_GetScancodeFromKey(key, NULL);
    event.key.key = key;
    event.key.mod = mod;
    event.key.down = down;
    event.key.repeat = repeat;
    return dispatch_input_event(&event);
}

bool
scrcpy_embedded_text(const char *text) {
    if (!text || !*text) {
        return false;
    }
    SDL_Event event = {0};
    event.text.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = text;
    return dispatch_input_event(&event);
}

static void
complete_server_wait(bool connected) {
    pthread_mutex_lock(&mutex);
    server_connected = connected;
    server_result_ready = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

static void
complete_session(enum scrcpy_exit_code result) {
    pthread_mutex_lock(&mutex);
    session_result = result;
    session_result_ready = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

enum scrcpy_embedded_status
scrcpy_embedded_pump(void) {
    assert(SDL_IsMainThread());
    if (!initialized) {
        return SCRCPY_EMBEDDED_FAILED;
    }

    SDL_Event event;
    // Do not call SDL_PollEvent() here: it pumps Cocoa events and removes
    // them from NSApplication before the SwiftUI host can dispatch them to
    // the embedded NSView. scrcpy's worker threads already enqueue their
    // events via SDL_PushEvent(), so reading the queue is sufficient.
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT,
                          SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0) {
        pthread_mutex_lock(&mutex);
        enum sc_embedded_phase current_phase = phase;
        struct sc_screen *screen = active_screen;
        pthread_mutex_unlock(&mutex);

        switch (event.type) {
            case SC_EVENT_SERVER_CONNECTED:
                complete_server_wait(true);
                continue;
            case SC_EVENT_SERVER_CONNECTION_FAILED:
                complete_server_wait(false);
                continue;
            case SDL_EVENT_QUIT:
                if (current_phase == SC_EMBEDDED_PHASE_WAIT_SERVER) {
                    complete_server_wait(false);
                } else if (current_phase == SC_EMBEDDED_PHASE_SESSION) {
                    complete_session(SCRCPY_EXIT_SUCCESS);
                }
                continue;
            case SC_EVENT_DEVICE_DISCONNECTED:
                if (screen && !screen->disconnected) {
                    sc_screen_handle_event(screen, &event);
                }
                complete_session(SCRCPY_EXIT_DISCONNECTED);
                continue;
            case SC_EVENT_DEMUXER_ERROR:
            case SC_EVENT_CONTROLLER_ERROR:
            case SC_EVENT_RECORDER_ERROR:
            case SC_EVENT_AOA_OPEN_ERROR:
                complete_session(SCRCPY_EXIT_FAILURE);
                continue;
            case SC_EVENT_TIME_LIMIT_REACHED:
                complete_session(SCRCPY_EXIT_SUCCESS);
                continue;
            default:
                if (screen) {
                    sc_screen_handle_event(screen, &event);
                }
                break;
        }
    }

    return scrcpy_embedded_get_status();
}

bool
sc_embedded_await_for_server(bool *connected) {
    pthread_mutex_lock(&mutex);
    while (!server_result_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    if (connected) {
        *connected = server_connected && !stop_requested;
    }
    bool result = server_connected || stop_requested;
    pthread_mutex_unlock(&mutex);
    return result;
}

void
sc_embedded_set_screen(struct sc_screen *screen) {
    pthread_mutex_lock(&mutex);
    active_screen = screen;
    pthread_mutex_unlock(&mutex);
}

enum scrcpy_exit_code
sc_embedded_event_loop(struct sc_screen *screen, bool has_screen) {
    pthread_mutex_lock(&mutex);
    active_screen = has_screen ? screen : NULL;
    phase = SC_EMBEDDED_PHASE_SESSION;
    status = SCRCPY_EMBEDDED_RUNNING;

    if (stop_requested) {
        session_result_ready = true;
        session_result = SCRCPY_EXIT_SUCCESS;
    }
    while (!session_result_ready) {
        pthread_cond_wait(&cond, &mutex);
    }
    enum scrcpy_exit_code result = session_result;
    pthread_mutex_unlock(&mutex);
    return result;
}

struct screen_init_context {
    struct sc_screen *screen;
    const struct sc_screen_params *params;
    bool result;
};

static void
screen_init_on_main(void *userdata) {
    struct screen_init_context *ctx = userdata;
    ctx->result = sc_screen_init(ctx->screen, ctx->params);
}

bool
sc_embedded_screen_init(struct sc_screen *screen,
                        const struct sc_screen_params *params) {
    struct screen_init_context ctx = {
        .screen = screen,
        .params = params,
        .result = false,
    };
    if (pthread_main_np()) {
        screen_init_on_main(&ctx);
    } else {
        dispatch_sync_f(dispatch_get_main_queue(), &ctx, screen_init_on_main);
    }
    return ctx.result;
}

static void
screen_destroy_on_main(void *userdata) {
    sc_screen_destroy(userdata);
}

void
sc_embedded_screen_destroy(struct sc_screen *screen) {
    if (pthread_main_np()) {
        screen_destroy_on_main(screen);
    } else {
        dispatch_sync_f(dispatch_get_main_queue(), screen,
                        screen_destroy_on_main);
    }
}
