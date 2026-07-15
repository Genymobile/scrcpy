#ifndef SC_EMBEDDED_H
#define SC_EMBEDDED_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "scrcpy.h"
#include "screen.h"

enum scrcpy_embedded_status {
    SCRCPY_EMBEDDED_IDLE = 0,
    SCRCPY_EMBEDDED_STARTING = 1,
    SCRCPY_EMBEDDED_RUNNING = 2,
    SCRCPY_EMBEDDED_FAILED = 3,
    SCRCPY_EMBEDDED_DISCONNECTED = 4,
};

struct scrcpy_embedded_session;

// Public API used by the macOS SwiftUI host. A session owns exactly one
// scrcpy connection and render surface. Pump and input functions must run on
// the main thread; different sessions may run concurrently.
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

// Redraw the last decoded frame. This is used by native hosts after Cocoa may
// have discarded the backing surface while the Android display was static.
// Must be called from the main thread.
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

bool
sc_embedded_is_enabled(void);

void *
sc_embedded_current_session(void);

void
sc_embedded_get_host(void *session, void **nswindow, void **nsview);

bool
sc_embedded_post_event(void *session, uint32_t type, void *data);

bool
sc_embedded_await_for_server(void *session, bool *connected);

void
sc_embedded_set_screen(void *session, struct sc_screen *screen);

enum scrcpy_exit_code
sc_embedded_event_loop(void *session, struct sc_screen *screen,
                       bool has_screen);

bool
sc_embedded_screen_init(struct sc_screen *screen,
                        const struct sc_screen_params *params);

void
sc_embedded_screen_destroy(struct sc_screen *screen);

#endif
