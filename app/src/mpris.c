#include "mpris.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL3/SDL_events.h>

#include "controller.h"
#include "events.h"
#include "util/log.h"

#define DOWNCAST(LISTENER) \
    container_of(LISTENER, struct sc_mpris, media_listener)

#define SC_MPRIS_BUS_NAME "org.mpris.MediaPlayer2.scrcpy"
#define SC_MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define SC_MPRIS_ROOT_INTERFACE "org.mpris.MediaPlayer2"
#define SC_MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define SC_MPRIS_TRACK_ID "/org/mpris/MediaPlayer2/scrcpy/track/0"

// Timeout of one iteration of the D-Bus loop (the stop request is checked
// after each iteration)
#define SC_MPRIS_LOOP_TIMEOUT_MS 100

// A D-Bus property of one of the MPRIS interfaces
struct sc_mpris_property {
    const char *name;
    // Append the value (wrapped in a variant), return false on OOM
    bool (*append)(DBusMessageIter *iter, const struct sc_mpris *mpris);
    bool notify; // included in PropertiesChanged on every player update
};

static const char *
playback_status_name(enum sc_media_state state) {
    switch (state) {
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

// Append a basic value (pointed to by value, as expected by libdbus) wrapped
// in a variant
static bool
append_variant(DBusMessageIter *iter, int type, const void *value) {
    char signature[2] = {type, '\0'};
    DBusMessageIter var;
    return dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, signature,
                                            &var)
        && dbus_message_iter_append_basic(&var, type, value)
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

    const char *track_id = SC_MPRIS_TRACK_ID;
    if (!open_dict_entry(&dict, &entry, "mpris:trackid")
            || !append_variant(&entry, DBUS_TYPE_OBJECT_PATH, &track_id)
            || !dbus_message_iter_close_container(&dict, &entry)) {
        return false;
    }

    if (player->duration_ms != SC_MEDIA_DURATION_UNKNOWN) {
        dbus_int64_t length_us = player->duration_ms * 1000;
        if (!open_dict_entry(&dict, &entry, "mpris:length")
                || !append_variant(&entry, DBUS_TYPE_INT64, &length_us)
                || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }
    if (player->title) {
        if (!open_dict_entry(&dict, &entry, "xesam:title")
                || !append_variant(&entry, DBUS_TYPE_STRING, &player->title)
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
                || !append_variant(&entry, DBUS_TYPE_STRING, &player->album)
                || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }

    return dbus_message_iter_close_container(&var, &dict)
        && dbus_message_iter_close_container(iter, &var);
}

// Property values (the mutex must be locked)

static bool
prop_true(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    (void) mpris;
    dbus_bool_t value = TRUE;
    return append_variant(iter, DBUS_TYPE_BOOLEAN, &value);
}

static bool
prop_false(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    (void) mpris;
    dbus_bool_t value = FALSE;
    return append_variant(iter, DBUS_TYPE_BOOLEAN, &value);
}

static bool
prop_one(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    (void) mpris;
    double value = 1.0;
    return append_variant(iter, DBUS_TYPE_DOUBLE, &value);
}

static bool
prop_identity(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    (void) mpris;
    const char *value = "scrcpy";
    return append_variant(iter, DBUS_TYPE_STRING, &value);
}

static bool
prop_no_strings(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    (void) mpris;
    return append_variant_strv(iter, NULL, 0);
}

static bool
prop_loop_status(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    (void) mpris;
    const char *value = "None";
    return append_variant(iter, DBUS_TYPE_STRING, &value);
}

static bool
prop_playback_status(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    const char *value = playback_status_name(mpris->player.state);
    return append_variant(iter, DBUS_TYPE_STRING, &value);
}

static bool
prop_metadata(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    return append_metadata(iter, &mpris->player);
}

static bool
prop_position(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    dbus_int64_t value = player_position_us(&mpris->player);
    return append_variant(iter, DBUS_TYPE_INT64, &value);
}

static bool
prop_capability(DBusMessageIter *iter, const struct sc_mpris *mpris,
                enum sc_media_cap cap) {
    dbus_bool_t value = !!(mpris->player.capabilities & cap);
    return append_variant(iter, DBUS_TYPE_BOOLEAN, &value);
}

static bool
prop_can_next(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    return prop_capability(iter, mpris, SC_MEDIA_CAP_NEXT);
}

static bool
prop_can_previous(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    return prop_capability(iter, mpris, SC_MEDIA_CAP_PREVIOUS);
}

static bool
prop_can_play(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    return prop_capability(iter, mpris, SC_MEDIA_CAP_PLAY);
}

static bool
prop_can_pause(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    return prop_capability(iter, mpris, SC_MEDIA_CAP_PAUSE);
}

static bool
prop_can_seek(DBusMessageIter *iter, const struct sc_mpris *mpris) {
    return prop_capability(iter, mpris, SC_MEDIA_CAP_SEEK);
}

static const struct sc_mpris_property root_properties[] = {
    {.name = "CanQuit", .append = prop_true},
    {.name = "CanSetFullscreen", .append = prop_false},
    {.name = "CanRaise", .append = prop_true},
    {.name = "Fullscreen", .append = prop_false},
    {.name = "HasTrackList", .append = prop_false},
    {.name = "Identity", .append = prop_identity},
    {.name = "DesktopEntry", .append = prop_identity},
    {.name = "SupportedUriSchemes", .append = prop_no_strings},
    {.name = "SupportedMimeTypes", .append = prop_no_strings},
};

static const struct sc_mpris_property player_properties[] = {
    {.name = "PlaybackStatus", .append = prop_playback_status, .notify = true},
    {.name = "LoopStatus", .append = prop_loop_status},
    {.name = "Rate", .append = prop_one},
    {.name = "Shuffle", .append = prop_false},
    {.name = "Metadata", .append = prop_metadata, .notify = true},
    {.name = "Volume", .append = prop_one},
    {.name = "Position", .append = prop_position},
    {.name = "MinimumRate", .append = prop_one},
    {.name = "MaximumRate", .append = prop_one},
    {.name = "CanGoNext", .append = prop_can_next, .notify = true},
    {.name = "CanGoPrevious", .append = prop_can_previous, .notify = true},
    {.name = "CanPlay", .append = prop_can_play, .notify = true},
    {.name = "CanPause", .append = prop_can_pause, .notify = true},
    {.name = "CanSeek", .append = prop_can_seek, .notify = true},
    {.name = "CanControl", .append = prop_true},
};

// Return the properties of the interface (NULL if the interface is unknown)
static const struct sc_mpris_property *
get_properties(const char *interface, size_t *count) {
    if (!strcmp(interface, SC_MPRIS_ROOT_INTERFACE)) {
        *count = ARRAY_LEN(root_properties);
        return root_properties;
    }
    if (!strcmp(interface, SC_MPRIS_PLAYER_INTERFACE)) {
        *count = ARRAY_LEN(player_properties);
        return player_properties;
    }
    return NULL;
}

static const struct sc_mpris_property *
find_property(const struct sc_mpris_property *properties, size_t count,
              const char *name) {
    for (size_t i = 0; i < count; ++i) {
        if (!strcmp(properties[i].name, name)) {
            return &properties[i];
        }
    }
    return NULL;
}

// Append a a{sv} array with the properties (all of them, or only the ones to
// notify)
// The mutex must be locked
static bool
append_properties(DBusMessageIter *iter, const struct sc_mpris *mpris,
                  const struct sc_mpris_property *properties, size_t count,
                  bool only_notify) {
    DBusMessageIter dict;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}",
                                          &dict)) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        const struct sc_mpris_property *prop = &properties[i];
        if (only_notify && !prop->notify) {
            continue;
        }
        DBusMessageIter entry;
        if (!open_dict_entry(&dict, &entry, prop->name)
                || !prop->append(&entry, mpris)
                || !dbus_message_iter_close_container(&dict, &entry)) {
            return false;
        }
    }

    return dbus_message_iter_close_container(iter, &dict);
}

// Signals (called from the receiver thread, the mutex must be locked)
//
// The messages are only queued: they are written by the D-Bus loop in the
// mpris thread

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
           && append_properties(&iter, mpris, player_properties,
                                ARRAY_LEN(player_properties), true)
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

// Method calls (called from the mpris thread)

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
        LOGW("Could not request media action");
    }
}

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

    size_t count;
    const struct sc_mpris_property *properties =
        get_properties(interface, &count);
    if (!properties) {
        return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_INTERFACE,
                           "Unknown interface");
    }

    const struct sc_mpris_property *prop =
        find_property(properties, count, name);
    if (!prop) {
        return reply_error(mpris, msg, DBUS_ERROR_UNKNOWN_PROPERTY,
                           "Unknown property");
    }

    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) {
        return send_reply(mpris, NULL);
    }

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);

    sc_mutex_lock(&mpris->mutex);
    bool ok = prop->append(&iter, mpris);
    sc_mutex_unlock(&mpris->mutex);

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

    size_t count;
    const struct sc_mpris_property *properties =
        get_properties(interface, &count);
    if (!properties) {
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
    bool ok = append_properties(&iter, mpris, properties, count, false);
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
    LOGD("MPRIS method: %s", member);

    if (!strcmp(member, "Raise")) {
        sc_push_event(SC_EVENT_RAISE_WINDOW);
    } else if (!strcmp(member, "Quit")) {
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
    LOGD("MPRIS method: %s", member);

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
        const char *track_id;
        dbus_int64_t requested_us;
        if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH,
                                   &track_id, DBUS_TYPE_INT64, &requested_us,
                                   DBUS_TYPE_INVALID)) {
            return reply_error(mpris, msg, DBUS_ERROR_INVALID_ARGS,
                               "Expected (ox)");
        }
        bool in_range = requested_us >= 0
                     && (duration_ms == SC_MEDIA_DURATION_UNKNOWN
                         || requested_us <= duration_ms * 1000);
        // As specified by MPRIS, ignore the call if the track id does not
        // match or if the position is out of range
        if (strcmp(track_id, SC_MPRIS_TRACK_ID) || !in_range) {
            LOGD("SetPosition ignored (track id or position mismatch)");
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

static int
run_mpris(void *data) {
    struct sc_mpris *mpris = data;

    while (!atomic_load(&mpris->stopped)) {
        // Read and write the connection, dispatch one message if any
        if (!dbus_connection_read_write_dispatch(mpris->conn,
                                                 SC_MPRIS_LOOP_TIMEOUT_MS)) {
            LOGW("Disconnected from the D-Bus session bus");
            break;
        }
    }

    return 0;
}

// sc_media_listener implementation (called from the receiver thread)

static void
sc_mpris_on_update(struct sc_media_listener *listener,
                   enum sc_media_state state, int64_t position_ms,
                   uint32_t capabilities, int64_t duration_ms, char *title,
                   char *artist, char *album) {
    struct sc_mpris *mpris = DOWNCAST(listener);
    struct sc_mpris_player *player = &mpris->player;

    sc_mutex_lock(&mpris->mutex);

    player->state = state;
    player->position_ms = position_ms;
    player->position_tick = sc_tick_now();
    player->capabilities = capabilities;
    player->duration_ms = duration_ms;

    // Take ownership of the strings
    free(player->title);
    free(player->artist);
    free(player->album);
    player->title = title;
    player->artist = artist;
    player->album = album;

    emit_player_properties_changed(mpris);
    if (position_ms != SC_MEDIA_POSITION_UNKNOWN) {
        // Let the clients resynchronize their position
        emit_seeked(mpris, player_position_us(player));
    }

    sc_mutex_unlock(&mpris->mutex);
}

static bool
request_bus_name(DBusConnection *conn) {
    // If the generic name is owned by another scrcpy instance, use an
    // instance-specific name (as allowed by the MPRIS specification)
    char instance_name[128];
    snprintf(instance_name, sizeof(instance_name),
             SC_MPRIS_BUS_NAME ".instance%ld", (long) getpid());
    const char *const names[] = {SC_MPRIS_BUS_NAME, instance_name};

    for (size_t i = 0; i < ARRAY_LEN(names); ++i) {
        DBusError err;
        dbus_error_init(&err);
        int r = dbus_bus_request_name(conn, names[i],
                                      DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
        if (dbus_error_is_set(&err)) {
            LOGE("Could not request D-Bus name: %s", err.message);
            dbus_error_free(&err);
            return false;
        }
        if (r == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
            LOGD("D-Bus name acquired: %s", names[i]);
            return true;
        }
    }

    LOGE("Could not acquire D-Bus name " SC_MPRIS_BUS_NAME);
    return false;
}

bool
sc_mpris_init(struct sc_mpris *mpris, struct sc_controller *controller) {
    static const struct sc_media_listener_ops ops = {
        .on_update = sc_mpris_on_update,
    };

    mpris->media_listener.ops = &ops;
    mpris->controller = controller;
    atomic_init(&mpris->stopped, false);

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

    // The connection is used from the receiver thread (signals) and from the
    // mpris thread (requests)
    if (!dbus_threads_init_default()) {
        LOGE("Could not initialize libdbus threading");
        goto error_destroy_mutex;
    }

    DBusError err;
    dbus_error_init(&err);
    DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (!conn) {
        LOGE("Could not connect to the D-Bus session bus: %s", err.message);
        dbus_error_free(&err);
        goto error_destroy_mutex;
    }

    dbus_connection_set_exit_on_disconnect(conn, FALSE);

    if (!request_bus_name(conn)) {
        goto error_close_conn;
    }

    if (!dbus_connection_try_register_object_path(conn, SC_MPRIS_OBJECT_PATH,
                                                  &object_vtable, mpris,
                                                  &err)) {
        LOGE("Could not register MPRIS object path: %s", err.message);
        dbus_error_free(&err);
        goto error_close_conn;
    }

    mpris->conn = conn;
    return true;

error_close_conn:
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
error_destroy_mutex:
    sc_mutex_destroy(&mpris->mutex);
    return false;
}

bool
sc_mpris_start(struct sc_mpris *mpris) {
    bool ok = sc_thread_create(&mpris->thread, run_mpris, "scrcpy-mpris",
                               mpris);
    if (!ok) {
        LOGE("Could not start MPRIS thread");
        return false;
    }

    return true;
}

void
sc_mpris_stop(struct sc_mpris *mpris) {
    // The thread checks the flag after each loop iteration
    atomic_store(&mpris->stopped, true);
}

void
sc_mpris_join(struct sc_mpris *mpris) {
    sc_thread_join(&mpris->thread, NULL);
}

void
sc_mpris_destroy(struct sc_mpris *mpris) {
    dbus_connection_unregister_object_path(mpris->conn, SC_MPRIS_OBJECT_PATH);
    // Private connection: close and unref
    dbus_connection_close(mpris->conn);
    dbus_connection_unref(mpris->conn);

    free(mpris->player.title);
    free(mpris->player.artist);
    free(mpris->player.album);

    sc_mutex_destroy(&mpris->mutex);
}
