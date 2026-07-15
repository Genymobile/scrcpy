#ifndef SCRCPY_DESK_PUBLIC_EMBED_H
#define SCRCPY_DESK_PUBLIC_EMBED_H

#include <stdbool.h>
#include <stdint.h>

enum scrcpy_embedded_status {
    SCRCPY_EMBEDDED_IDLE = 0,
    SCRCPY_EMBEDDED_STARTING = 1,
    SCRCPY_EMBEDDED_RUNNING = 2,
    SCRCPY_EMBEDDED_FAILED = 3,
    SCRCPY_EMBEDDED_DISCONNECTED = 4,
};

bool
scrcpy_embedded_initialize(void *nswindow, void *nsview,
                           const char *adb_path, const char *server_path,
                           const char *icon_dir);

bool
scrcpy_embedded_start(const char *serial);

void
scrcpy_embedded_stop(void);

enum scrcpy_embedded_status
scrcpy_embedded_pump(void);

enum scrcpy_embedded_status
scrcpy_embedded_get_status(void);

bool
scrcpy_embedded_mouse_motion(float x, float y, float xrel, float yrel,
                             uint32_t buttons);

bool
scrcpy_embedded_mouse_button(bool down, uint8_t button, float x, float y,
                             uint8_t clicks);

bool
scrcpy_embedded_mouse_wheel(float scroll_x, float scroll_y,
                            float mouse_x, float mouse_y);

bool
scrcpy_embedded_key(bool down, const char *key_name,
                    bool shift, bool control, bool alt, bool command,
                    bool repeat);

bool
scrcpy_embedded_text(const char *text);

#endif
