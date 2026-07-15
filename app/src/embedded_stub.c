#include "embedded.h"

bool
scrcpy_embedded_initialize(void *nswindow, void *nsview,
                           const char *adb_path, const char *server_path,
                           const char *icon_dir) {
    (void) nswindow;
    (void) nsview;
    (void) adb_path;
    (void) server_path;
    (void) icon_dir;
    return false;
}

bool
scrcpy_embedded_start(const char *serial) {
    (void) serial;
    return false;
}

void
scrcpy_embedded_stop(void) {
}

enum scrcpy_embedded_status
scrcpy_embedded_pump(void) {
    return SCRCPY_EMBEDDED_FAILED;
}

enum scrcpy_embedded_status
scrcpy_embedded_get_status(void) {
    return SCRCPY_EMBEDDED_IDLE;
}

bool
scrcpy_embedded_mouse_motion(float x, float y, float xrel, float yrel,
                             uint32_t buttons) {
    (void) x;
    (void) y;
    (void) xrel;
    (void) yrel;
    (void) buttons;
    return false;
}

bool
scrcpy_embedded_mouse_button(bool down, uint8_t button, float x, float y,
                             uint8_t clicks) {
    (void) down;
    (void) button;
    (void) x;
    (void) y;
    (void) clicks;
    return false;
}

bool
scrcpy_embedded_mouse_wheel(float scroll_x, float scroll_y,
                            float mouse_x, float mouse_y) {
    (void) scroll_x;
    (void) scroll_y;
    (void) mouse_x;
    (void) mouse_y;
    return false;
}

bool
scrcpy_embedded_key(bool down, const char *key_name,
                    bool shift, bool control, bool alt, bool command,
                    bool repeat) {
    (void) down;
    (void) key_name;
    (void) shift;
    (void) control;
    (void) alt;
    (void) command;
    (void) repeat;
    return false;
}

bool
scrcpy_embedded_text(const char *text) {
    (void) text;
    return false;
}

bool
sc_embedded_is_enabled(void) {
    return false;
}

bool
sc_embedded_await_for_server(bool *connected) {
    (void) connected;
    return false;
}

void
sc_embedded_set_screen(struct sc_screen *screen) {
    (void) screen;
}

enum scrcpy_exit_code
sc_embedded_event_loop(struct sc_screen *screen, bool has_screen) {
    (void) screen;
    (void) has_screen;
    return SCRCPY_EXIT_FAILURE;
}

bool
sc_embedded_screen_init(struct sc_screen *screen,
                        const struct sc_screen_params *params) {
    (void) screen;
    (void) params;
    return false;
}

void
sc_embedded_screen_destroy(struct sc_screen *screen) {
    (void) screen;
}
