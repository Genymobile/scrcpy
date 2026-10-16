#ifndef SC_MPRIS_H
#define SC_MPRIS_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>

#include <dbus/dbus.h>

#include "trait/media_listener.h"
#include "util/thread.h"
#include "util/tick.h"
#include "util/vector.h"

struct sc_controller;

// The media session of the device exposed via MPRIS
struct sc_mpris_player {
    enum sc_media_state state;
    int64_t position_ms; // SC_MEDIA_POSITION_UNKNOWN if unknown
    sc_tick position_tick; // when position_ms was received
    uint32_t capabilities; // OR of enum sc_media_cap values
    int64_t duration_ms; // SC_MEDIA_DURATION_UNKNOWN if unknown
    char *title; // owned, may be NULL
    char *artist; // owned, may be NULL
    char *album; // owned, may be NULL
};

// Expose the media session of the device on the D-Bus session bus, using the
// MPRIS interface <https://specifications.freedesktop.org/mpris-spec/latest/>.
//
// The media updates are received from the receiver thread (via the
// sc_media_listener trait), which updates the player and emits the D-Bus
// signals. The incoming D-Bus requests (method calls and property accesses)
// are handled by a dedicated thread. The controller is used to send the media
// actions to the device.
struct sc_mpris {
    struct sc_media_listener media_listener; // internal

    struct sc_controller *controller;

    sc_thread thread;
    atomic_bool stopped;

    // Private connection to the session bus
    DBusConnection *conn;

    // Pipe to wake up the thread (from libdbus or on stop)
    int wakeup_pipe[2];

    // Watches registered by libdbus (may be modified from any thread sending
    // a message, protected by watches_mutex)
    sc_mutex watches_mutex;
    struct SC_VECTOR(DBusWatch *) watches;

    sc_mutex mutex;
    struct sc_mpris_player player; // protected by mutex
};

bool
sc_mpris_init(struct sc_mpris *mpris, struct sc_controller *controller);

bool
sc_mpris_start(struct sc_mpris *mpris);

void
sc_mpris_stop(struct sc_mpris *mpris);

void
sc_mpris_join(struct sc_mpris *mpris);

void
sc_mpris_destroy(struct sc_mpris *mpris);

#endif
