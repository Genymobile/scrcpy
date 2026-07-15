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

struct scrcpy_embedded_session;

struct scrcpy_embedded_session *
scrcpy_embedded_session_create(void *nswindow, void *nsview,
                               const char *adb_path,
                               const char *server_path,
                               const char *icon_dir);

void
scrcpy_embedded_session_destroy(struct scrcpy_embedded_session *session);

bool
scrcpy_embedded_session_start(struct scrcpy_embedded_session *session,
                              const char *serial);

void
scrcpy_embedded_session_stop(struct scrcpy_embedded_session *session);

enum scrcpy_embedded_status
scrcpy_embedded_session_pump(struct scrcpy_embedded_session *session);

enum scrcpy_embedded_status
scrcpy_embedded_session_get_status(struct scrcpy_embedded_session *session);

bool
scrcpy_embedded_session_refresh(struct scrcpy_embedded_session *session);

bool
scrcpy_embedded_session_mouse_motion(struct scrcpy_embedded_session *session,
                                     float x, float y, float xrel, float yrel,
                                     uint32_t buttons);

bool
scrcpy_embedded_session_mouse_button(struct scrcpy_embedded_session *session,
                                     bool down, uint8_t button,
                                     float x, float y, uint8_t clicks);

bool
scrcpy_embedded_session_mouse_wheel(struct scrcpy_embedded_session *session,
                                    float scroll_x, float scroll_y,
                                    float mouse_x, float mouse_y);

bool
scrcpy_embedded_session_key(struct scrcpy_embedded_session *session,
                            bool down, const char *key_name,
                            bool shift, bool control, bool alt, bool command,
                            bool repeat);

bool
scrcpy_embedded_session_text(struct scrcpy_embedded_session *session,
                             const char *text);

#endif
