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

struct scrcpy_embedded_session {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t thread;
    bool thread_joinable;
    bool stop_requested;
    bool server_result_ready;
    bool server_connected;
    bool session_result_ready;
    enum scrcpy_exit_code session_result;
    enum sc_embedded_phase phase;
    enum scrcpy_embedded_status status;
    struct sc_screen *active_screen;
    void *nswindow;
    void *nsview;
    struct scrcpy_embedded_session *next;
};

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool global_initialized;
static struct scrcpy_embedded_session *sessions;
static _Thread_local struct scrcpy_embedded_session *current_session;

bool
sc_embedded_is_enabled(void) {
    pthread_mutex_lock(&global_mutex);
    bool enabled = global_initialized;
    pthread_mutex_unlock(&global_mutex);
    return enabled;
}

void *
sc_embedded_current_session(void) {
    return current_session;
}

void
sc_embedded_get_host(void *opaque, void **nswindow, void **nsview) {
    struct scrcpy_embedded_session *session = opaque;
    if (nswindow) {
        *nswindow = session ? session->nswindow : NULL;
    }
    if (nsview) {
        *nsview = session ? session->nsview : NULL;
    }
}

static bool
initialize_global(const char *adb_path, const char *server_path,
                  const char *icon_dir) {
    assert(SDL_IsMainThread());

    pthread_mutex_lock(&global_mutex);
    if (global_initialized) {
        pthread_mutex_unlock(&global_mutex);
        return true;
    }

    if (setenv("ADB", adb_path, 1)
            || setenv("SCRCPY_SERVER_PATH", server_path, 1)
            || (icon_dir && setenv("SCRCPY_ICON_DIR", icon_dir, 1))) {
        pthread_mutex_unlock(&global_mutex);
        return false;
    }

    sc_sdl_set_embedded_mode(true);
    sc_sdl_set_hints(NULL, false);
    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        LOGE("Could not initialize embedded SDL: %s", SDL_GetError());
        pthread_mutex_unlock(&global_mutex);
        return false;
    }
    if (!net_init() || !sc_main_thread_init()) {
        pthread_mutex_unlock(&global_mutex);
        return false;
    }

    global_initialized = true;
    pthread_mutex_unlock(&global_mutex);
    return true;
}

struct scrcpy_embedded_session *
scrcpy_embedded_session_create(void *nswindow, void *nsview,
                               const char *adb_path,
                               const char *server_path,
                               const char *icon_dir) {
    if (!nswindow || !nsview || !adb_path || !server_path
            || !initialize_global(adb_path, server_path, icon_dir)) {
        return NULL;
    }

    struct scrcpy_embedded_session *session = calloc(1, sizeof(*session));
    if (!session) {
        return NULL;
    }
    if (pthread_mutex_init(&session->mutex, NULL)) {
        free(session);
        return NULL;
    }
    if (pthread_cond_init(&session->cond, NULL)) {
        pthread_mutex_destroy(&session->mutex);
        free(session);
        return NULL;
    }

    session->phase = SC_EMBEDDED_PHASE_IDLE;
    session->status = SCRCPY_EMBEDDED_IDLE;
    session->nswindow = nswindow;
    session->nsview = nsview;

    pthread_mutex_lock(&global_mutex);
    session->next = sessions;
    sessions = session;
    pthread_mutex_unlock(&global_mutex);
    return session;
}

static void *
run_session(void *userdata) {
    struct {
        struct scrcpy_embedded_session *session;
        char *serial;
    } *ctx = userdata;
    struct scrcpy_embedded_session *session = ctx->session;
    char *serial = ctx->serial;
    free(ctx);

    current_session = session;
    struct scrcpy_options options = scrcpy_options_default;
    options.serial = serial;
    options.audio_source = SC_AUDIO_SOURCE_OUTPUT;
    options.audio_codec = SC_CODEC_AAC;
    options.keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_SDK;
    options.mouse_input_mode = SC_MOUSE_INPUT_MODE_SDK;
    options.gamepad_input_mode = SC_GAMEPAD_INPUT_MODE_DISABLED;
    options.window_aspect_ratio_lock = false;
    options.render_fit = SC_RENDER_FIT_LETTERBOX;
    options.update_terminal_title = false;

    enum scrcpy_exit_code result = scrcpy(&options);
    free(serial);
    current_session = NULL;

    pthread_mutex_lock(&session->mutex);
    session->active_screen = NULL;
    session->phase = SC_EMBEDDED_PHASE_IDLE;
    if (result == SCRCPY_EXIT_DISCONNECTED) {
        session->status = SCRCPY_EMBEDDED_DISCONNECTED;
    } else if (result == SCRCPY_EXIT_FAILURE) {
        session->status = SCRCPY_EMBEDDED_FAILED;
    } else {
        session->status = SCRCPY_EMBEDDED_IDLE;
    }
    pthread_cond_broadcast(&session->cond);
    pthread_mutex_unlock(&session->mutex);
    return NULL;
}

bool
scrcpy_embedded_session_start(struct scrcpy_embedded_session *session,
                              const char *serial) {
    if (!session || !serial || !*serial) {
        return false;
    }

    pthread_mutex_lock(&session->mutex);
    bool busy = session->phase != SC_EMBEDDED_PHASE_IDLE
             || session->status == SCRCPY_EMBEDDED_STARTING
             || session->status == SCRCPY_EMBEDDED_RUNNING;
    pthread_mutex_unlock(&session->mutex);
    if (busy) {
        return false;
    }

    if (session->thread_joinable) {
        pthread_join(session->thread, NULL);
        session->thread_joinable = false;
    }

    char *serial_copy = strdup(serial);
    void *raw_ctx = malloc(sizeof(struct {
        struct scrcpy_embedded_session *session;
        char *serial;
    }));
    if (!serial_copy || !raw_ctx) {
        free(serial_copy);
        free(raw_ctx);
        return false;
    }
    struct {
        struct scrcpy_embedded_session *session;
        char *serial;
    } *ctx = raw_ctx;
    ctx->session = session;
    ctx->serial = serial_copy;

    pthread_mutex_lock(&session->mutex);
    session->stop_requested = false;
    session->server_result_ready = false;
    session->server_connected = false;
    session->session_result_ready = false;
    session->session_result = SCRCPY_EXIT_FAILURE;
    session->active_screen = NULL;
    session->phase = SC_EMBEDDED_PHASE_WAIT_SERVER;
    session->status = SCRCPY_EMBEDDED_STARTING;
    pthread_mutex_unlock(&session->mutex);

    if (pthread_create(&session->thread, NULL, run_session, ctx)) {
        pthread_mutex_lock(&session->mutex);
        session->phase = SC_EMBEDDED_PHASE_IDLE;
        session->status = SCRCPY_EMBEDDED_FAILED;
        pthread_mutex_unlock(&session->mutex);
        free(serial_copy);
        free(ctx);
        return false;
    }
    session->thread_joinable = true;
    return true;
}

void
scrcpy_embedded_session_stop(struct scrcpy_embedded_session *session) {
    if (!session) {
        return;
    }
    pthread_mutex_lock(&session->mutex);
    session->stop_requested = true;
    if (session->phase == SC_EMBEDDED_PHASE_WAIT_SERVER) {
        session->server_connected = false;
        session->server_result_ready = true;
    } else if (session->phase == SC_EMBEDDED_PHASE_SESSION) {
        session->session_result = SCRCPY_EXIT_SUCCESS;
        session->session_result_ready = true;
    }
    pthread_cond_broadcast(&session->cond);
    pthread_mutex_unlock(&session->mutex);
}

enum scrcpy_embedded_status
scrcpy_embedded_session_get_status(struct scrcpy_embedded_session *session) {
    if (!session) {
        return SCRCPY_EMBEDDED_FAILED;
    }
    pthread_mutex_lock(&session->mutex);
    enum scrcpy_embedded_status status = session->status;
    pthread_mutex_unlock(&session->mutex);
    return status;
}

static struct sc_screen *
get_active_screen(struct scrcpy_embedded_session *session) {
    pthread_mutex_lock(&session->mutex);
    struct sc_screen *screen = session->status == SCRCPY_EMBEDDED_RUNNING
                             ? session->active_screen : NULL;
    pthread_mutex_unlock(&session->mutex);
    return screen;
}

static bool
dispatch_input_event(struct scrcpy_embedded_session *session,
                     SDL_Event *event) {
    assert(SDL_IsMainThread());
    if (!session) {
        return false;
    }
    struct sc_screen *screen = get_active_screen(session);
    if (!screen || screen->disconnected) {
        return false;
    }
    event->button.windowID = SDL_GetWindowID(screen->window);
    sc_screen_handle_event(screen, event);
    return true;
}

bool
scrcpy_embedded_session_mouse_motion(struct scrcpy_embedded_session *session,
                                     float x, float y, float xrel, float yrel,
                                     uint32_t buttons) {
    SDL_Event event = {0};
    event.motion.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.state = buttons;
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    return dispatch_input_event(session, &event);
}

bool
scrcpy_embedded_session_mouse_button(struct scrcpy_embedded_session *session,
                                     bool down, uint8_t button,
                                     float x, float y, uint8_t clicks) {
    SDL_Event event = {0};
    event.button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN
                             : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = button;
    event.button.down = down;
    event.button.clicks = clicks;
    event.button.x = x;
    event.button.y = y;
    return dispatch_input_event(session, &event);
}

bool
scrcpy_embedded_session_mouse_wheel(struct scrcpy_embedded_session *session,
                                    float scroll_x, float scroll_y,
                                    float mouse_x, float mouse_y) {
    SDL_Event event = {0};
    event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.x = scroll_x;
    event.wheel.y = scroll_y;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouse_x = mouse_x;
    event.wheel.mouse_y = mouse_y;
    return dispatch_input_event(session, &event);
}

bool
scrcpy_embedded_session_key(struct scrcpy_embedded_session *session,
                            bool down, const char *key_name,
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
    if (shift) mod |= SDL_KMOD_LSHIFT;
    if (control) mod |= SDL_KMOD_LCTRL;
    if (alt) mod |= SDL_KMOD_LALT;
    if (command) mod |= SDL_KMOD_LGUI;
    SDL_SetModState(mod);

    SDL_Event event = {0};
    event.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = SDL_GetScancodeFromKey(key, NULL);
    event.key.key = key;
    event.key.mod = mod;
    event.key.down = down;
    event.key.repeat = repeat;
    return dispatch_input_event(session, &event);
}

bool
scrcpy_embedded_session_text(struct scrcpy_embedded_session *session,
                             const char *text) {
    if (!text || !*text) {
        return false;
    }
    SDL_Event event = {0};
    event.text.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = text;
    return dispatch_input_event(session, &event);
}

bool
sc_embedded_post_event(void *opaque, uint32_t type, void *data) {
    if (!opaque) {
        return sc_push_event_impl(type, data, "embedded event");
    }
    SDL_Event event = {
        .user = {
            .type = type,
            .data1 = data,
            .data2 = opaque,
        },
    };
    bool ok = SDL_PushEvent(&event);
    if (!ok) {
        LOGE("Could not post embedded event: %s", SDL_GetError());
    }
    return ok;
}

static void
complete_server_wait(struct scrcpy_embedded_session *session,
                     bool connected) {
    pthread_mutex_lock(&session->mutex);
    session->server_connected = connected;
    session->server_result_ready = true;
    pthread_cond_broadcast(&session->cond);
    pthread_mutex_unlock(&session->mutex);
}

static void
complete_session(struct scrcpy_embedded_session *session,
                 enum scrcpy_exit_code result) {
    pthread_mutex_lock(&session->mutex);
    session->session_result = result;
    session->session_result_ready = true;
    pthread_cond_broadcast(&session->cond);
    pthread_mutex_unlock(&session->mutex);
}

static void
handle_queued_event(SDL_Event *event) {
    bool window_event = event->type >= SDL_EVENT_WINDOW_FIRST
                     && event->type <= SDL_EVENT_WINDOW_LAST;
    bool render_event = event->type >= SDL_EVENT_RENDER_TARGETS_RESET
                     && event->type <= SDL_EVENT_RENDER_DEVICE_LOST;
    if (window_event || render_event) {
        // SDL uses one process-wide queue. Find the embedded screen owning
        // this native window instead of dropping its window/render lifecycle
        // events. These events are essential on macOS because the window
        // server may discard a Metal drawable while the mirrored Android
        // screen is static, leaving the host view black until its last frame
        // is drawn again.
        uint32_t window_id = window_event ? event->window.windowID
                                          : event->render.windowID;
        pthread_mutex_lock(&global_mutex);
        struct scrcpy_embedded_session *it = sessions;
        while (it) {
            pthread_mutex_lock(&it->mutex);
            struct sc_screen *screen = it->active_screen;
            bool matches = screen
                        && SDL_GetWindowID(screen->window) == window_id;
            pthread_mutex_unlock(&it->mutex);
            if (matches) {
                sc_screen_handle_event(screen, event);
                break;
            }
            it = it->next;
        }
        pthread_mutex_unlock(&global_mutex);
        return;
    }

    if (event->type < SC_EVENT_NEW_FRAME
            || event->type > SC_EVENT_DISCONNECTED_TIMEOUT) {
        // Native Cocoa input is forwarded directly by the SwiftUI host. Do
        // not interpret unrelated SDL events as tagged embedded events (their
        // user.data2 bytes are not a session pointer).
        return;
    }
    struct scrcpy_embedded_session *session = event->user.data2;
    if (!session) {
        return;
    }
    pthread_mutex_lock(&session->mutex);
    struct sc_screen *screen = session->active_screen;
    pthread_mutex_unlock(&session->mutex);

    switch (event->type) {
        case SC_EVENT_SERVER_CONNECTED:
            complete_server_wait(session, true);
            break;
        case SC_EVENT_SERVER_CONNECTION_FAILED:
            complete_server_wait(session, false);
            break;
        case SC_EVENT_DEVICE_DISCONNECTED:
            if (screen && !screen->disconnected) {
                sc_screen_handle_event(screen, event);
            }
            complete_session(session, SCRCPY_EXIT_DISCONNECTED);
            break;
        case SC_EVENT_DEMUXER_ERROR:
        case SC_EVENT_CONTROLLER_ERROR:
        case SC_EVENT_RECORDER_ERROR:
        case SC_EVENT_AOA_OPEN_ERROR:
            complete_session(session, SCRCPY_EXIT_FAILURE);
            break;
        case SC_EVENT_TIME_LIMIT_REACHED:
            complete_session(session, SCRCPY_EXIT_SUCCESS);
            break;
        default:
            if (screen) {
                sc_screen_handle_event(screen, event);
            }
            break;
    }
}

bool
scrcpy_embedded_session_refresh(struct scrcpy_embedded_session *session) {
    assert(SDL_IsMainThread());
    if (!session) {
        return false;
    }

    struct sc_screen *screen = get_active_screen(session);
    if (!screen || !screen->window_shown) {
        return false;
    }

    SDL_Event event = {
        .window = {
            .type = SDL_EVENT_WINDOW_EXPOSED,
            .windowID = SDL_GetWindowID(screen->window),
        },
    };
    sc_screen_handle_event(screen, &event);
    return true;
}

enum scrcpy_embedded_status
scrcpy_embedded_session_pump(struct scrcpy_embedded_session *session) {
    assert(SDL_IsMainThread());
    if (!session || !global_initialized) {
        return SCRCPY_EMBEDDED_FAILED;
    }
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT,
                          SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0) {
        handle_queued_event(&event);
    }
    return scrcpy_embedded_session_get_status(session);
}

bool
sc_embedded_await_for_server(void *opaque, bool *connected) {
    struct scrcpy_embedded_session *session = opaque;
    assert(session);
    pthread_mutex_lock(&session->mutex);
    while (!session->server_result_ready) {
        pthread_cond_wait(&session->cond, &session->mutex);
    }
    if (connected) {
        *connected = session->server_connected && !session->stop_requested;
    }
    bool result = session->server_connected || session->stop_requested;
    pthread_mutex_unlock(&session->mutex);
    return result;
}

void
sc_embedded_set_screen(void *opaque, struct sc_screen *screen) {
    struct scrcpy_embedded_session *session = opaque;
    assert(session);
    pthread_mutex_lock(&session->mutex);
    session->active_screen = screen;
    pthread_mutex_unlock(&session->mutex);
}

enum scrcpy_exit_code
sc_embedded_event_loop(void *opaque, struct sc_screen *screen,
                       bool has_screen) {
    struct scrcpy_embedded_session *session = opaque;
    assert(session);
    pthread_mutex_lock(&session->mutex);
    session->active_screen = has_screen ? screen : NULL;
    session->phase = SC_EMBEDDED_PHASE_SESSION;
    session->status = SCRCPY_EMBEDDED_RUNNING;
    if (session->stop_requested) {
        session->session_result_ready = true;
        session->session_result = SCRCPY_EXIT_SUCCESS;
    }
    while (!session->session_result_ready) {
        pthread_cond_wait(&session->cond, &session->mutex);
    }
    enum scrcpy_exit_code result = session->session_result;
    pthread_mutex_unlock(&session->mutex);
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

static void
destroy_session_on_main(void *userdata) {
    struct scrcpy_embedded_session *session = userdata;
    assert(SDL_IsMainThread());

    // No producer can enqueue more events after the worker has joined. Drain
    // the shared queue once so no event keeps a pointer to the freed session.
    scrcpy_embedded_session_pump(session);

    pthread_mutex_lock(&global_mutex);
    struct scrcpy_embedded_session **it = &sessions;
    while (*it && *it != session) {
        it = &(*it)->next;
    }
    if (*it == session) {
        *it = session->next;
    }
    pthread_mutex_unlock(&global_mutex);

    pthread_cond_destroy(&session->cond);
    pthread_mutex_destroy(&session->mutex);
    free(session);
}

void
scrcpy_embedded_session_destroy(struct scrcpy_embedded_session *session) {
    if (!session) {
        return;
    }
    scrcpy_embedded_session_stop(session);
    if (session->thread_joinable) {
        pthread_join(session->thread, NULL);
    }

    // Session shutdown may wait for Cocoa/SDL cleanup on the main thread.
    // Consequently the macOS host destroys sessions from a worker queue so
    // removing a card never blocks SwiftUI. Final queue draining and freeing
    // still happen on the main thread, where SDL event access is required.
    if (pthread_main_np()) {
        destroy_session_on_main(session);
    } else {
        dispatch_sync_f(dispatch_get_main_queue(), session,
                        destroy_session_on_main);
    }
}
