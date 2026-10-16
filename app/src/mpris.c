#include "mpris.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL3/SDL_events.h>

#include "controller.h"
#include "events.h"
#include "util/log.h"
#include "util/tick.h"

#define SC_MPRIS_BUS_NAME "org.mpris.MediaPlayer2.scrcpy"
#define SC_MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define SC_MPRIS_ROOT_INTERFACE "org.mpris.MediaPlayer2"
#define SC_MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define SC_MPRIS_TRACK_ID "/org/mpris/MediaPlayer2/scrcpy/track/0"

static const char *const root_properties[] = {
    "CanQuit",
    "CanSetFullscreen",
    "CanRaise",
    "Fullscreen",
    "HasTrackList",
    "Identity",
    "DesktopEntry",
    "SupportedUriSchemes",
    "SupportedMimeTypes",
};

static const char *const player_properties[] = {
    "PlaybackStatus",
    "LoopStatus",
    "Rate",
    "Shuffle",
    "Metadata",
    "Volume",
    "Position",
    "MinimumRate",
    "MaximumRate",
    "CanGoNext",
    "CanGoPrevious",
    "CanPlay",
    "CanPause",
    "CanSeek",
    "CanControl",
};

// The player properties emitted in PropertiesChanged on every update
static const char *const player_changed_properties[] = {
    "PlaybackStatus",
    "Metadata",
    "CanGoNext",
    "CanGoPrevious",
    "CanPlay",
    "CanPause",
    "CanSeek",
};

static const char *
playback_status_name(const struct sc_mpris_player *player) {
    switch (player->state) {
        case SC_MEDIA_STATE_PLAYING:
            return "Playing";
        case SC_MEDIA_STATE_PAUSED:
            return "Paused";
        default:
            return "Stopped";
    }
}

// Return the current position (in microseconds), extrapolated from the last
// position update if the player is playing
static int64_t
player_position_us(const struct sc_mpris_player *player) {
    if (player->position_ms == SC_MEDIA_POSITION_UNKNOWN) {
        return 0;
    }

    int64_t position_us = player->position_ms * 1000;
    if (player->state == SC_MEDIA_STATE_PLAYING) {
        sc_tick now = sc_tick_now();
        position_us += SC_TICK_TO_US(now - player->position_tick);
    }

    if (player->duration_ms != SC_MEDIA_DURATION_UNKNOWN) {
        int64_t duration_us = player->duration_ms * 1000;
        if (position_us > duration_us) {
            position_us = duration_us;
        }
    }

    if (position_us < 0) {
        position_us = 0;
    }

    return position_us;
}

// D-Bus marshalling helpers (return false on OOM)

static bool
append_variant_string(DBusMessageIter *iter, const char *value) {
    DBusMessageIter var;
    return dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
                                            DBUS_TYPE_STRING_AS_STRING, &var)
        && dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &value)
        && dbus_message_iter_close_container(iter, &var);
}

static bool
append_variant_object_path(DBusMessageIter *iter, const char *value) {
    DBusMessageIter var;
    return dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
                                            DBUS_TYPE_OBJECT_PATH_AS_STRING,
                                            &var)
        && dbus_message_iter_append_basic(&var, DBUS_TYPE_OBJECT_PATH, &value)
        && dbus_message_iter_close_container(iter, &var);
}

static bool
append_variant_bool(DBusMessageIter *iter, bool value) {
    dbus_bool_t b = value;
    DBusMessageIter var;
    return dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
                                            DBUS_TYPE_BOOLEAN_AS_STRING, &var)
        && dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &b)
        && dbus_message_iter_close_container(iter, &var);
}

static bool
append_variant_double(DBusMessageIter *iter, double value) {
    DBusMessageIter var;
    return dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
                                            DBUS_TYPE_DOUBLE_AS_STRING, &var)
        && dbus_message_iter_append_basic(&var, DBUS_TYPE_DOUBLE, &value)
        && dbus_message_iter_close_container(iter, &var);
}

static bool
append_variant_int64(DBusMessageIter *iter, int64_t value) {
    dbus_int64_t v = value;
    DBusMessageIter var;
    return dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
                                            DBUS_TYPE_INT64_AS_STRING, &var)
        && dbus_message_iter_append_basic(&var, DBUS_TYPE_INT64, &v)
        && dbus_message_iter_close_container(iter, &var);
}

// Append a variant containing an array of strings
static bool
append_variant_strv(DBusMessageIter *iter, const char *const *values,
                    size_t count) {
    DBusMessageIter var;
    DBusMessageIter array;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "as", &var)
            || !dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY,
                                                 DBUS_TYPE_STRING_AS_STRING,
                                                 &array)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING,
                                            &values[i])) {
            return false;
        }
    }
    return dbus_message_iter_close_container(&var, &array)
        && dbus_message_iter_close_container(iter, &var);
}

// Open a dict entry in a a{sv} array and append its key
static bool
open_dict_entry(DBusMessageIter *array, DBusMessageIter *entry,
                const char *key) {
    return dbus_message_iter_open_container(array, DBUS_TYPE_DICT_ENTRY, NULL,
                                            entry)
        && dbus_message_iter_append_basic(entry, DBUS_TYPE_STRING, &key);
}

// Append the MPRIS metadata (a{sv}) wrapped in a variant
static bool
append_metadata(DBusMessageIter *iter, const struct sc_mpris_player *player) {
    DBusMessageIter var;
    DBusMessageIter dict;
    DBusMessageIter entry;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "a{sv}",
                                          &var)
            || !dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "{sv}",
                                                 &dict)) {
        return false;
    }

    if (!open_dict_entry(&dict, &entry, "mpris:trackid")
            || !append_variant_object_path(&entry, SC_MPRIS_TRACK_ID)
            || !dbus_message_iter_close_container(&dict, &entry)) {
        return false;
    }

    if (player->duration_ms != SC_MEDIA_DURATION_UNKNOWN) {
        if (!open_dict_entry(&dict, &entry, "mpris:length")
                || !append_variant_int64(&entry, player->duration_ms * 1000)
                || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }
    if (player->title) {
        if (!open_dict_entry(&dict, &entry, "xesam:title")
                || !append_variant_string(&entry, player->title)
                || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }
    if (player->artist) {
        const char *const artists[] = {player->artist};
        if (!open_dict_entry(&dict, &entry, "xesam:artist")
                || !append_variant_strv(&entry, artists, 1)
                || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }
    if (player->album) {
        if (!open_dict_entry(&dict, &entry, "xesam:album")
                || !append_variant_string(&entry, player->album)
                || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }

    return dbus_message_iter_close_container(&var, &dict)
        && dbus_message_iter_close_container(iter, &var);
}

// Properties

// Append the value of a root property (as a variant)
// Return false if the property is unknown
static bool
append_root_property(DBusMessageIter *iter, const char *name, bool *ok) {
    if (!strcmp(name, "CanQuit") || !strcmp(name, "CanRaise")) {
        *ok = append_variant_bool(iter, true);
    } else if (!strcmp(name, "CanSetFullscreen") || !strcmp(name, "Fullscreen")
            || !strcmp(name, "HasTrackList")) {
        *ok = append_variant_bool(iter, false);
    } else if (!strcmp(name, "Identity") || !strcmp(name, "DesktopEntry")) {
        *ok = append_variant_string(iter, "scrcpy");
    } else if (!strcmp(name, "SupportedUriSchemes")
            || !strcmp(name, "SupportedMimeTypes")) {
        *ok = append_variant_strv(iter, NULL, 0);
    } else {
        return false;
    }
    return true;
}

// Append the value of a player property (as a variant)
// Return false if the property is unknown
// The mutex must be locked
static bool
append_player_property(DBusMessageIter *iter, const struct sc_mpris *mpris,
                       const char *name, bool *ok) {
    const struct sc_mpris_player *player = &mpris->player;
    uint32_t caps = player->capabilities;

    if (!strcmp(name, "PlaybackStatus")) {
        *ok = append_variant_string(iter, playback_status_name(player));
    } else if (!strcmp(name, "LoopStatus")) {
        *ok = append_variant_string(iter, "None");
    } else if (!strcmp(name, "Rate") || !strcmp(name, "Volume")
            || !strcmp(name, "MinimumRate") || !strcmp(name, "MaximumRate")) {
        *ok = append_variant_double(iter, 1.0);
    } else if (!strcmp(name, "Shuffle")) {
        *ok = append_variant_bool(iter, false);
    } else if (!strcmp(name, "Metadata")) {
        *ok = append_metadata(iter, player);
    } else if (!strcmp(name, "Position")) {
        *ok = append_variant_int64(iter, player_position_us(player));
    } else if (!strcmp(name, "CanGoNext")) {
        *ok = append_variant_bool(iter, caps & SC_MEDIA_CAP_NEXT);
    } else if (!strcmp(name, "CanGoPrevious")) {
        *ok = append_variant_bool(iter, caps & SC_MEDIA_CAP_PREVIOUS);
    } else if (!strcmp(name, "CanPlay")) {
        *ok = append_variant_bool(iter, caps & SC_MEDIA_CAP_PLAY);
    } else if (!strcmp(name, "CanPause")) {
        *ok = append_variant_bool(iter, caps & SC_MEDIA_CAP_PAUSE);
    } else if (!strcmp(name, "CanSeek")) {
        *ok = append_variant_bool(iter, caps & SC_MEDIA_CAP_SEEK);
    } else if (!strcmp(name, "CanControl")) {
        *ok = append_variant_bool(iter, true);
    } else {
        return false;
    }
    return true;
}

// Append a a{sv} array with the requested properties of an interface
// The mutex must be locked
static bool
append_properties(DBusMessageIter *iter, struct sc_mpris *mpris,
                  const char *interface, const char *const *names,
                  size_t count) {
    DBusMessageIter dict;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}",
                                          &dict)) {
        return false;
    }

    bool is_root = !strcmp(interface, SC_MPRIS_ROOT_INTERFACE);
    for (size_t i = 0; i < count; ++i) {
        DBusMessageIter entry;
        if (!open_dict_entry(&dict, &entry, names[i])) {
            return false;
        }
        bool ok;
        bool known = is_root
                   ? append_root_property(&entry, names[i], &ok)
                   : append_player_property(&entry, mpris, names[i], &ok);
        assert(known);
        (void) known;
        if (!ok || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }

    return dbus_message_iter_close_container(iter, &dict);
}

// Signals (called from the receiver thread, the mutex must be locked)

static void
send_message(struct sc_mpris *mpris, DBusMessage *msg) {
    if (!dbus_connection_send(mpris->conn, msg, NULL)) {
        LOG_OOM();
    }
    dbus_message_unref(msg);
}

static void
emit_player_properties_changed(struct sc_mpris *mpris) {
    DBusMessage *msg = dbus_message_new_signal(SC_MPRIS_OBJECT_PATH,
                                               DBUS_INTERFACE_PROPERTIES,
                                               "PropertiesChanged");
    if (!msg) {
        LOG_OOM();
        return;
    }

    const char *interface = SC_MPRIS_PLAYER_INTERFACE;
    DBusMessageIter iter;
    DBusMessageIter invalidated;
    dbus_message_iter_init_append(msg, &iter);
    bool ok = dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING,
                                             &interface)
           && append_properties(&iter, mpris, interface,
                                player_changed_properties,
                                ARRAY_LEN(player_changed_properties))
           && dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                               DBUS_TYPE_STRING_AS_STRING,
                                               &invalidated)
           && dbus_message_iter_close_container(&iter, &invalidated);
    if (!ok) {
        LOG_OOM();
        dbus_message_unref(msg);
        return;
    }

    send_message(mpris, msg);
}

static void
emit_seeked(struct sc_mpris *mpris, int64_t position_us) {
    DBusMessage *msg = dbus_message_new_signal(SC_MPRIS_OBJECT_PATH,
                                               SC_MPRIS_PLAYER_INTERFACE,
                                               "Seeked");
    if (!msg) {
        LOG_OOM();
        return;
    }

    dbus_int64_t position = position_us;
    if (!dbus_message_append_args(msg, DBUS_TYPE_INT64, &position,
                                  DBUS_TYPE_INVALID)) {
        LOG_OOM();
        dbus_message_unref(msg);
        return;
    }

    send_message(mpris, msg);
}

// Actions (called from the mpris thread)

static void
send_action(struct sc_mpris *mpris, enum sc_media_action action,
            int64_t position_us) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_MEDIA_ACTION,
        .media_action = {
            .action = action,
            .position_ms = MAX(position_us, 0) / 1000,
        },
    };

    if (!sc_controller_push_msg(mpris->controller, &msg)) {
        LOGW("mpris: could not request media action");
    }
}

// Method calls (called from the mpris thread)

static DBusHandlerResult
send_reply(struct sc_mpris *mpris, DBusMessage *reply) {
    if (!reply) {
        LOG_OOM();
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }
    if (!dbus_connection_send(mpris->conn, reply, NULL)) {
        LOG_OOM();
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
reply_empty(struct sc_mpris *mpris, DBusMessage *msg) {
    return send_reply(mpris, dbus_message_new_method_return(msg));
}

static DBusHandlerResult
reply_error(struct sc_mpris *mpris, DBusMessage *msg, const char *error,
            const char *text) {
    return send_reply(mpris, dbus_message_new_error(msg, error, text));
}

static DBusHandlerResult
handle_properties_get(struct sc_mpris *mpris, DBusMessage *msg) {
    const char *interface;
    const char *name;
    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &interface,
                               DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID)) {
        return reply_error(mpris, msg, DBUS_ERROR_INVALID_ARGS,
                           "Expected (ss)");
    }

    bool is_root = !strcmp(interface, SC_MPRIS_ROOT_INTERFACE);
    if (!is_root && strcmp(interface, SC_MPRIS_PLAYER_INTERFACE)) {
        return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_INTERFACE,
                           "Unknown interface");
    }

    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) {
        return send_reply(mpris, NULL);
    }

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);

    bool known;
    bool ok = true;
    if (is_root) {
        known = append_root_property(&iter, name, &ok);
    } else {
        sc_mutex_lock(&mpris->mutex);
        known = append_player_property(&iter, mpris, name, &ok);
        sc_mutex_unlock(&mpris->mutex);
    }

    if (!known) {
        dbus_message_unref(reply);
        return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_PROPERTY,
                           "Unknown property");
    }
    if (!ok) {
        dbus_message_unref(reply);
        return send_reply(mpris, NULL);
    }

    return send_reply(mpris, reply);
}

static DBusHandlerResult
handle_properties_get_all(struct sc_mpris *mpris, DBusMessage *msg) {
    const char *interface;
    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &interface,
                               DBUS_TYPE_INVALID)) {
        return reply_error(mpris, msg, DBUS_ERROR_INVALID_ARGS,
                           "Expected (s)");
    }

    const char *const *names;
    size_t count;
    if (!strcmp(interface, SC_MPRIS_ROOT_INTERFACE)) {
        names = root_properties;
        count = ARRAY_LEN(root_properties);
    } else if (!strcmp(interface, SC_MPRIS_PLAYER_INTERFACE)) {
        names = player_properties;
        count = ARRAY_LEN(player_properties);
    } else {
        return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_INTERFACE,
                           "Unknown interface");
    }

    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) {
        return send_reply(mpris, NULL);
    }

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);

    sc_mutex_lock(&mpris->mutex);
    bool ok = append_properties(&iter, mpris, interface, names, count);
    sc_mutex_unlock(&mpris->mutex);

    if (!ok) {
        dbus_message_unref(reply);
        return send_reply(mpris, NULL);
    }

    return send_reply(mpris, reply);
}

static DBusHandlerResult
handle_root_method(struct sc_mpris *mpris, DBusMessage *msg,
                   const char *member) {
    if (!strcmp(member, "Raise")) {
        LOGD("mpris: raise");
        sc_push_event(SC_EVENT_RAISE_WINDOW);
    } else if (!strcmp(member, "Quit")) {
        LOGD("mpris: quit");
        sc_push_event(SDL_EVENT_QUIT);
    } else {
        return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_METHOD,
                           "Unknown method");
    }

    return reply_empty(mpris, msg);
}

static DBusHandlerResult
handle_player_method(struct sc_mpris *mpris, DBusMessage *msg,
                     const char *member) {
    LOGD("mpris: %s", member);

    if (!strcmp(member, "OpenUri")) {
        return reply_error(mpris, msg, DBUS_ERROR_NOT_SUPPORTED,
                           "OpenUri is not supported");
    }

    // Snapshot of the player
    sc_mutex_lock(&mpris->mutex);
    enum sc_media_state state = mpris->player.state;
    int64_t position_us = player_position_us(&mpris->player);
    int64_t duration_ms = mpris->player.duration_ms;
    sc_mutex_unlock(&mpris->mutex);

    enum sc_media_action action;
    int64_t target_us = 0;
    if (!strcmp(member, "Play")) {
        action = SC_MEDIA_ACTION_PLAY;
    } else if (!strcmp(member, "Pause")) {
        action = SC_MEDIA_ACTION_PAUSE;
    } else if (!strcmp(member, "PlayPause")) {
        action = state == SC_MEDIA_STATE_PLAYING ? SC_MEDIA_ACTION_PAUSE
                                                 : SC_MEDIA_ACTION_PLAY;
    } else if (!strcmp(member, "Stop")) {
        action = SC_MEDIA_ACTION_STOP;
    } else if (!strcmp(member, "Next")) {
        action = SC_MEDIA_ACTION_NEXT;
    } else if (!strcmp(member, "Previous")) {
        action = SC_MEDIA_ACTION_PREVIOUS;
    } else if (!strcmp(member, "Seek")) {
        dbus_int64_t offset_us;
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_INT64, &offset_us,
                                   DBUS_TYPE_INVALID)) {
            return reply_error(mpris, msg, DBUS_ERROR_INVALID_ARGS,
                               "Expected (x)");
        }
        target_us = position_us + offset_us;
        if (duration_ms != SC_MEDIA_DURATION_UNKNOWN
                && target_us > duration_ms * 1000) {
            // As specified by MPRIS: seeking beyond the end of the track acts
            // like a call to Next
            action = SC_MEDIA_ACTION_NEXT;
        } else {
            action = SC_MEDIA_ACTION_SEEK;
        }
    } else if (!strcmp(member, "SetPosition")) {
        const char *requested_track_id;
        dbus_int64_t requested_us;
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH,
                                   &requested_track_id, DBUS_TYPE_INT64,
                                   &requested_us, DBUS_TYPE_INVALID)) {
            return reply_error(mpris, msg, DBUS_ERROR_INVALID_ARGS,
                               "Expected (ox)");
        }
        bool same_track = !strcmp(requested_track_id, SC_MPRIS_TRACK_ID);
        bool in_range = requested_us >= 0
                     && (duration_ms == SC_MEDIA_DURATION_UNKNOWN
                         || requested_us <= duration_ms * 1000);
        // As specified by MPRIS, ignore the call if the track id does not
        // match or if the position is out of range
        if (!same_track || !in_range) {
            LOGD("mpris: SetPosition ignored");
            return reply_empty(mpris, msg);
        }
        action = SC_MEDIA_ACTION_SEEK;
        target_us = requested_us;
    } else {
        return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_METHOD,
                           "Unknown method");
    }

    send_action(mpris, action, target_us);
    return reply_empty(mpris, msg);
}

static DBusHandlerResult
handle_message(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    (void) conn;
    struct sc_mpris *mpris = userdata;

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    const char *interface = dbus_message_get_interface(msg);
    const char *member = dbus_message_get_member(msg);
    if (!member) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // The interface is optional in D-Bus method calls
    bool any = !interface;

    if (any || !strcmp(interface, DBUS_INTERFACE_PROPERTIES)) {
        if (!strcmp(member, "Get")) {
            return handle_properties_get(mpris, msg);
        }
        if (!strcmp(member, "GetAll")) {
            return handle_properties_get_all(mpris, msg);
        }
        if (!strcmp(member, "Set")) {
            // The writable MPRIS properties (LoopStatus, Rate, Shuffle,
            // Volume, Fullscreen) have no equivalent in the Android media
            // session API
            return reply_error(mpris, msg, DBUS_ERROR_NOT_SUPPORTED,
                               "Setting properties is not supported");
        }
    }
    if (any || !strcmp(interface, SC_MPRIS_ROOT_INTERFACE)) {
        if (!strcmp(member, "Raise") || !strcmp(member, "Quit")) {
            return handle_root_method(mpris, msg, member);
        }
    }
    if (any || !strcmp(interface, SC_MPRIS_PLAYER_INTERFACE)) {
        return handle_player_method(mpris, msg, member);
    }

    return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_METHOD,
                       "Unknown method");
}

static const DBusObjectPathVTable object_vtable = {
    .message_function = handle_message,
};

// Main loop integration (watches and wakeup)

static void
wakeup(struct sc_mpris *mpris) {
    uint8_t byte = 0;
    ssize_t r = write(mpris->wakeup_pipe[1], &byte, 1);
    (void) r; // EAGAIN (pipe full) is fine, the thread will wake up anyway
}

static void
wakeup_main(void *data) {
    wakeup(data);
}

static dbus_bool_t
add_watch(DBusWatch *watch, void *data) {
    struct sc_mpris *mpris = data;
    sc_mutex_lock(&mpris->watches_mutex);
    bool ok = sc_vector_push(&mpris->watches, watch);
    sc_mutex_unlock(&mpris->watches_mutex);
    if (!ok) {
        LOG_OOM();
    }
    return ok;
}

static void
remove_watch(DBusWatch *watch, void *data) {
    struct sc_mpris *mpris = data;
    sc_mutex_lock(&mpris->watches_mutex);
    ssize_t idx = sc_vector_index_of(&mpris->watches, watch);
    if (idx != -1) {
        sc_vector_swap_remove(&mpris->watches, idx);
    }
    sc_mutex_unlock(&mpris->watches_mutex);
}

static void
toggle_watch(DBusWatch *watch, void *data) {
    (void) watch;
    // The enabled state is checked on each loop iteration
    wakeup(data);
}

static bool
has_watch(struct sc_mpris *mpris, DBusWatch *watch) {
    sc_mutex_lock(&mpris->watches_mutex);
    bool found = sc_vector_index_of(&mpris->watches, watch) != -1;
    sc_mutex_unlock(&mpris->watches_mutex);
    return found;
}

static int
run_mpris(void *data) {
    struct sc_mpris *mpris = data;
    DBusConnection *conn = mpris->conn;

    // Request the bus name (blocking call)
    DBusError err;
    dbus_error_init(&err);
    int r = dbus_bus_request_name(conn, SC_MPRIS_BUS_NAME,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        LOGW("mpris: could not request bus name: %s", err.message);
        dbus_error_free(&err);
    } else if (r == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        LOGD("mpris: bus name acquired: " SC_MPRIS_BUS_NAME);
    } else {
        // The generic name is probably owned by another scrcpy instance, use
        // an instance-specific name (as allowed by the MPRIS specification)
        char name[128];
        snprintf(name, sizeof(name), SC_MPRIS_BUS_NAME ".instance%ld",
                 (long) getpid());
        r = dbus_bus_request_name(conn, name, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                  &err);
        if (dbus_error_is_set(&err)) {
            LOGW("mpris: could not request bus name: %s", err.message);
            dbus_error_free(&err);
        } else if (r == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
            LOGD("mpris: bus name acquired: %s", name);
        } else {
            LOGW("mpris: could not acquire bus name %s", name);
        }
    }

    // Snapshot of the enabled watches for one iteration (they may be
    // modified by libdbus while they are handled), and the matching pollfds
    // (with the wakeup pipe at index 0)
    struct SC_VECTOR(DBusWatch *) watches = SC_VECTOR_INITIALIZER;
    struct SC_VECTOR(struct pollfd) fds = SC_VECTOR_INITIALIZER;

    while (!atomic_load(&mpris->stopped)) {
        watches.size = 0;
        fds.size = 0;

        struct pollfd wakeup_fd = {
            .fd = mpris->wakeup_pipe[0],
            .events = POLLIN,
        };
        bool ok = sc_vector_push(&fds, wakeup_fd);

        sc_mutex_lock(&mpris->watches_mutex);
        for (size_t i = 0; ok && i < mpris->watches.size; ++i) {
            DBusWatch *watch = mpris->watches.data[i];
            if (!dbus_watch_get_enabled(watch)) {
                continue;
            }
            unsigned flags = dbus_watch_get_flags(watch);
            struct pollfd fd = {
                .fd = dbus_watch_get_unix_fd(watch),
                .events = (flags & DBUS_WATCH_READABLE ? POLLIN : 0)
                        | (flags & DBUS_WATCH_WRITABLE ? POLLOUT : 0),
            };
            ok = sc_vector_push(&watches, watch) && sc_vector_push(&fds, fd);
        }
        sc_mutex_unlock(&mpris->watches_mutex);

        if (!ok) {
            LOG_OOM();
            break;
        }

        int ret = poll(fds.data, fds.size, -1);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOGE("mpris: poll() failed: %s", strerror(errno));
            break;
        }

        if (fds.data[0].revents & POLLIN) {
            // Drain the wakeup pipe
            uint8_t buf[64];
            while (read(mpris->wakeup_pipe[0], buf, sizeof(buf)) > 0) {
                // continue
            }
        }

        for (size_t i = 0; i < watches.size; ++i) {
            short revents = fds.data[i + 1].revents;
            if (!revents) {
                continue;
            }
            DBusWatch *watch = watches.data[i];
            if (!has_watch(mpris, watch)) {
                // Removed while handling a previous watch
                continue;
            }
            unsigned flags = (revents & POLLIN ? DBUS_WATCH_READABLE : 0)
                           | (revents & POLLOUT ? DBUS_WATCH_WRITABLE : 0)
                           | (revents & POLLERR ? DBUS_WATCH_ERROR : 0)
                           | (revents & POLLHUP ? DBUS_WATCH_HANGUP : 0);
            if (!dbus_watch_handle(watch, flags)) {
                LOG_OOM();
            }
        }

        while (dbus_connection_get_dispatch_status(conn)
                == DBUS_DISPATCH_DATA_REMAINS) {
            dbus_connection_dispatch(conn);
        }

        if (!dbus_connection_get_is_connected(conn)) {
            LOGW("mpris: disconnected from the session bus");
            break;
        }
    }

    sc_vector_destroy(&watches);
    sc_vector_destroy(&fds);

    LOGD("mpris: thread stopped");
    return 0;
}

// sc_media_listener implementation (called from the receiver thread)

static void
sc_mpris_on_update(struct sc_media_listener *listener,
                   struct sc_media_update *update) {
    struct sc_mpris *mpris = container_of(listener, struct sc_mpris,
                                          media_listener);
    struct sc_mpris_player *player = &mpris->player;

    sc_mutex_lock(&mpris->mutex);

    player->state = update->state;
    player->position_ms = update->position_ms;
    player->position_tick = sc_tick_now();
    player->capabilities = update->capabilities;
    player->duration_ms = update->duration_ms;

    // Take ownership of the strings
    free(player->title);
    free(player->artist);
    free(player->album);
    player->title = update->title;
    player->artist = update->artist;
    player->album = update->album;

    emit_player_properties_changed(mpris);
    if (player->position_ms != SC_MEDIA_POSITION_UNKNOWN) {
        // Let the clients resynchronize their position
        emit_seeked(mpris, player_position_us(player));
    }

    sc_mutex_unlock(&mpris->mutex);
}

// Lifecycle

static bool
set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool
sc_mpris_init(struct sc_mpris *mpris, struct sc_controller *controller) {
    static const struct sc_media_listener_ops ops = {
        .on_update = sc_mpris_on_update,
    };

    mpris->media_listener.ops = &ops;
    mpris->controller = controller;
    atomic_init(&mpris->stopped, false);
    mpris->conn = NULL;
    mpris->wakeup_pipe[0] = -1;
    mpris->wakeup_pipe[1] = -1;
    sc_vector_init(&mpris->watches);

    // No media session yet
    mpris->player.state = SC_MEDIA_STATE_STOPPED;
    mpris->player.position_ms = SC_MEDIA_POSITION_UNKNOWN;
    mpris->player.capabilities = 0;
    mpris->player.duration_ms = SC_MEDIA_DURATION_UNKNOWN;
    mpris->player.title = NULL;
    mpris->player.artist = NULL;
    mpris->player.album = NULL;

    bool ok = sc_mutex_init(&mpris->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_mutex_init(&mpris->watches_mutex);
    if (!ok) {
        goto error_destroy_mutex;
    }

    // The connection is used from the receiver thread (signals) and from the
    // mpris thread (requests)
    if (!dbus_threads_init_default()) {
        LOGE("mpris: could not initialize libdbus threading");
        goto error_destroy_watches_mutex;
    }

    DBusError err;
    dbus_error_init(&err);
    DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (!conn) {
        // Not an error: the feature is just unavailable
        LOGW("mpris: could not connect to the session bus: %s", err.message);
        dbus_error_free(&err);
        return true;
    }

    dbus_connection_set_exit_on_disconnect(conn, FALSE);

    if (pipe(mpris->wakeup_pipe) == -1) {
        LOGE("mpris: could not create wakeup pipe: %s", strerror(errno));
        goto error_close_conn;
    }
    if (!set_nonblocking(mpris->wakeup_pipe[0])
            || !set_nonblocking(mpris->wakeup_pipe[1])) {
        LOGE("mpris: could not configure wakeup pipe: %s", strerror(errno));
        goto error_close_pipe;
    }

    if (!dbus_connection_try_register_object_path(conn, SC_MPRIS_OBJECT_PATH,
                                                  &object_vtable, mpris,
                                                  &err)) {
        LOGE("mpris: could not register object path: %s", err.message);
        dbus_error_free(&err);
        goto error_close_pipe;
    }

    if (!dbus_connection_set_watch_functions(conn, add_watch, remove_watch,
                                             toggle_watch, mpris, NULL)) {
        LOG_OOM();
        goto error_unregister;
    }
    dbus_connection_set_wakeup_main_function(conn, wakeup_main, mpris, NULL);

    mpris->conn = conn;
    return true;

error_unregister:
    dbus_connection_unregister_object_path(conn, SC_MPRIS_OBJECT_PATH);
error_close_pipe:
    close(mpris->wakeup_pipe[0]);
    close(mpris->wakeup_pipe[1]);
error_close_conn:
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    sc_vector_destroy(&mpris->watches);
error_destroy_watches_mutex:
    sc_mutex_destroy(&mpris->watches_mutex);
error_destroy_mutex:
    sc_mutex_destroy(&mpris->mutex);
    return false;
}

bool
sc_mpris_start(struct sc_mpris *mpris) {
    if (!mpris->conn) {
        // Feature unavailable, nothing to do
        return true;
    }

    bool ok = sc_thread_create(&mpris->thread, run_mpris, "scrcpy-mpris",
                               mpris);
    if (!ok) {
        LOGE("mpris: could not start thread");
        return false;
    }

    return true;
}

void
sc_mpris_stop(struct sc_mpris *mpris) {
    if (!mpris->conn) {
        return;
    }

    atomic_store(&mpris->stopped, true);
    wakeup(mpris);
}

void
sc_mpris_join(struct sc_mpris *mpris) {
    if (!mpris->conn) {
        return;
    }

    sc_thread_join(&mpris->thread, NULL);
}

void
sc_mpris_destroy(struct sc_mpris *mpris) {
    if (mpris->conn) {
        dbus_connection_unregister_object_path(mpris->conn,
                                               SC_MPRIS_OBJECT_PATH);
        // Private connection: close and unref
        dbus_connection_close(mpris->conn);
        dbus_connection_unref(mpris->conn);

        close(mpris->wakeup_pipe[0]);
        close(mpris->wakeup_pipe[1]);
        sc_vector_destroy(&mpris->watches);
    }

    free(mpris->player.title);
    free(mpris->player.artist);
    free(mpris->player.album);

    sc_mutex_destroy(&mpris->watches_mutex);
    sc_mutex_destroy(&mpris->mutex);
}
