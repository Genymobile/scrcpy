#include "embedded.h"

struct scrcpy_embedded_session *
scrcpy_embedded_session_create(void *nswindow, void *nsview,
                               const char *adb_path,
                               const char *server_path,
                               const char *icon_dir) {
    (void) nswindow; (void) nsview; (void) adb_path;
    (void) server_path; (void) icon_dir;
    return NULL;
}

void
scrcpy_embedded_session_destroy(struct scrcpy_embedded_session *session) {
    (void) session;
}

bool
scrcpy_embedded_session_start(struct scrcpy_embedded_session *session,
                              const char *serial) {
    (void) session; (void) serial;
    return false;
}

void
scrcpy_embedded_session_stop(struct scrcpy_embedded_session *session) {
    (void) session;
}

enum scrcpy_embedded_status
scrcpy_embedded_session_pump(struct scrcpy_embedded_session *session) {
    (void) session;
    return SCRCPY_EMBEDDED_FAILED;
}

enum scrcpy_embedded_status
scrcpy_embedded_session_get_status(struct scrcpy_embedded_session *session) {
    (void) session;
    return SCRCPY_EMBEDDED_IDLE;
}

bool
scrcpy_embedded_session_refresh(struct scrcpy_embedded_session *session) {
    (void) session;
    return false;
}

bool
scrcpy_embedded_session_mouse_motion(struct scrcpy_embedded_session *session,
                                     float x, float y, float xrel, float yrel,
                                     uint32_t buttons) {
    (void) session; (void) x; (void) y; (void) xrel; (void) yrel;
    (void) buttons;
    return false;
}

bool
scrcpy_embedded_session_mouse_button(struct scrcpy_embedded_session *session,
                                     bool down, uint8_t button,
                                     float x, float y, uint8_t clicks) {
    (void) session; (void) down; (void) button; (void) x; (void) y;
    (void) clicks;
    return false;
}

bool
scrcpy_embedded_session_mouse_wheel(struct scrcpy_embedded_session *session,
                                    float scroll_x, float scroll_y,
                                    float mouse_x, float mouse_y) {
    (void) session; (void) scroll_x; (void) scroll_y;
    (void) mouse_x; (void) mouse_y;
    return false;
}

bool
scrcpy_embedded_session_key(struct scrcpy_embedded_session *session,
                            bool down, const char *key_name,
                            bool shift, bool control, bool alt, bool command,
                            bool repeat) {
    (void) session; (void) down; (void) key_name; (void) shift;
    (void) control; (void) alt; (void) command; (void) repeat;
    return false;
}

bool
scrcpy_embedded_session_text(struct scrcpy_embedded_session *session,
                             const char *text) {
    (void) session; (void) text;
    return false;
}

bool sc_embedded_is_enabled(void) { return false; }
void *sc_embedded_current_session(void) { return NULL; }
void sc_embedded_get_host(void *s, void **w, void **v) {
    (void) s; if (w) *w = NULL; if (v) *v = NULL;
}
bool sc_embedded_post_event(void *s, uint32_t t, void *d) {
    (void) s; (void) t; (void) d; return false;
}
bool sc_embedded_await_for_server(void *s, bool *c) {
    (void) s; (void) c; return false;
}
void sc_embedded_set_screen(void *s, struct sc_screen *screen) {
    (void) s; (void) screen;
}
enum scrcpy_exit_code
sc_embedded_event_loop(void *s, struct sc_screen *screen, bool has_screen) {
    (void) s; (void) screen; (void) has_screen;
    return SCRCPY_EXIT_FAILURE;
}
bool sc_embedded_screen_init(struct sc_screen *screen,
                             const struct sc_screen_params *params) {
    (void) screen; (void) params; return false;
}
void sc_embedded_screen_destroy(struct sc_screen *screen) { (void) screen; }
