#include "mpris.h"
#include "events.h"
#include <SDL_events.h>
#include "util/log.h"
#include <gio/gio.h>
#include <glib-unix.h>
#include <string.h>

static const char *introspection_xml =
	"<node>\n"
	"  <interface name=\"org.mpris.MediaPlayer2\">\n"
	"    <method name=\"Raise\"/>\n"
	"    <method name=\"Quit\"/>\n"
	"    <property name=\"CanQuit\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"CanSetFullscreen\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"CanRaise\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"Fullscreen\" type=\"b\" access=\"readwrite\"/>\n"
	"    <property name=\"HasTrackList\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"Identity\" type=\"s\" access=\"read\"/>\n"
	"    <property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>\n"
	"    <property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>\n"
	"    <property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>\n"
	"  </interface>\n"
	"  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
	"    <method name=\"Next\" />\n"
	"    <method name=\"Previous\" />\n"
	"    <method name=\"Pause\" />\n"
	"    <method name=\"PlayPause\" />\n"
	"    <method name=\"Stop\" />\n"
	"    <method name=\"Play\" />\n"
	"    <method name=\"Seek\">\n"
	"      <arg type=\"x\" name=\"Offset\" direction=\"in\"/>\n"
	"    </method>\n"
	"    <method name=\"SetPosition\">\n"
	"      <arg type=\"o\" name=\"TrackId\" direction=\"in\"/>\n"
	"      <arg type=\"x\" name=\"Offset\" direction=\"in\"/>\n"
	"    </method>\n"
	"    <method name=\"OpenUri\">\n"
	"      <arg type=\"s\" name=\"Uri\" direction=\"in\"/>\n"
	"    </method>\n"
	"    <signal name=\"Seeked\">\n"
	"      <arg type=\"x\" name=\"Position\" direction=\"out\"/>\n"
	"    </signal>\n"
	"    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>\n"
	"    <property name=\"LoopStatus\" type=\"s\" access=\"readwrite\"/>\n"
	"    <property name=\"Rate\" type=\"d\" access=\"readwrite\"/>\n"
	"    <property name=\"Shuffle\" type=\"b\" access=\"readwrite\"/>\n"
	"    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>\n"
	"    <property name=\"Volume\" type=\"d\" access=\"readwrite\"/>\n"
	"    <property name=\"Position\" type=\"x\" access=\"read\"/>\n"
	"    <property name=\"MinimumRate\" type=\"d\" access=\"read\"/>\n"
	"    <property name=\"MaximumRate\" type=\"d\" access=\"read\"/>\n"
	"    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"CanPause\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"CanSeek\" type=\"b\" access=\"read\"/>\n"
	"    <property name=\"CanControl\" type=\"b\" access=\"read\"/>\n"
	"  </interface>\n"
	"</node>\n";

static inline void
push_event(uint32_t type, const char *name) {
	SDL_Event event;
	event.type = type;
	int ret = SDL_PushEvent(&event);
	if (ret < 0) {
		LOGE("Could not post %s event: %s", name, SDL_GetError());
		// What could we do?
	}
}
#define PUSH_EVENT(TYPE) push_event(TYPE, #TYPE)

static void
method_call_root(G_GNUC_UNUSED GDBusConnection *connection,
				 G_GNUC_UNUSED const char *sender,
				 G_GNUC_UNUSED const char *object_path,
				 G_GNUC_UNUSED const char *interface_name,
				 const char *method_name, G_GNUC_UNUSED GVariant *parameters,
				 GDBusMethodInvocation *invocation, G_GNUC_UNUSED gpointer user_data) {
	if (g_strcmp0(method_name, "Quit") == 0) {
        LOGD("mpris: quit");
		g_dbus_method_invocation_return_value(invocation, NULL);

	} else if (g_strcmp0(method_name, "Raise") == 0) {
        LOGD("mpris: raise window");
        PUSH_EVENT(SC_EVENT_RAISE_WINDOW);
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else {
        LOGW("mpris: unknown method %s", method_name);
		g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
											  G_DBUS_ERROR_UNKNOWN_METHOD,
											  "Unknown method");
	}
}

static GVariant *
get_property_root(G_GNUC_UNUSED GDBusConnection *connection,
				  G_GNUC_UNUSED const char *sender,
				  G_GNUC_UNUSED const char *object_path,
				  G_GNUC_UNUSED const char *interface_name,
				  const char *property_name, G_GNUC_UNUSED GError **error,
				  G_GNUC_UNUSED gpointer user_data) {
	GVariant *ret;

	printf("Getting property %s\n", property_name);
	if (g_strcmp0(property_name, "CanQuit") == 0) {
		ret = g_variant_new_boolean(FALSE);

	} else if (g_strcmp0(property_name, "Fullscreen") == 0) {
		int fullscreen = 0;
		ret = g_variant_new_boolean(fullscreen);
	} else if (g_strcmp0(property_name, "CanSetFullscreen") == 0) {
		ret = g_variant_new_boolean(FALSE);
	} else if (g_strcmp0(property_name, "CanRaise") == 0) {
		ret = g_variant_new_boolean(TRUE);
	} else if (g_strcmp0(property_name, "HasTrackList") == 0) {
		ret = g_variant_new_boolean(FALSE);
	} else if (g_strcmp0(property_name, "Identity") == 0) {
		ret = g_variant_new_string("Identity");
	} else if (g_strcmp0(property_name, "DesktopEntry") == 0) {
		ret = g_variant_new_string("scrcpy");
	} else if (g_strcmp0(property_name, "SupportedUriSchemes") == 0) {
		ret = NULL;
	} else if (g_strcmp0(property_name, "SupportedMimeTypes") == 0) {
		ret = NULL;
	} else {
		ret = NULL;
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
					"Unknown property %s", property_name);
	}

	return ret;
}

static gboolean
set_property_root(G_GNUC_UNUSED GDBusConnection *connection,
				  G_GNUC_UNUSED const char *sender,
				  G_GNUC_UNUSED const char *object_path,
				  G_GNUC_UNUSED const char *interface_name,
				  const char *property_name, GVariant *value,
				  G_GNUC_UNUSED GError **error, G_GNUC_UNUSED gpointer user_data) {
	if (g_strcmp0(property_name, "Fullscreen") == 0) {
		int fullscreen;
		g_variant_get(value, "b", &fullscreen);
        LOGD("mpris: setting fullscreen to %d", fullscreen);
	} else {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
					"Cannot set property %s", property_name);
		return FALSE;
	}
	return TRUE;
}

static GDBusInterfaceVTable vtable_root = {
	method_call_root, get_property_root, set_property_root, {0}};

static void
method_call_player(G_GNUC_UNUSED GDBusConnection *connection,
				   G_GNUC_UNUSED const char *sender,
				   G_GNUC_UNUSED const char *_object_path,
				   G_GNUC_UNUSED const char *interface_name,
				   const char *method_name, G_GNUC_UNUSED GVariant *parameters,
				   GDBusMethodInvocation *invocation, G_GNUC_UNUSED gpointer user_data) {
	if (g_strcmp0(method_name, "Pause") == 0) {
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "PlayPause") == 0) {
        LOGD("mpris: PlayPause");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "Play") == 0) {
        LOGD("mpris: Play");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "Stop") == 0) {
        LOGD("mpris: Stop");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "Next") == 0) {
        LOGD("mpris: Next");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "Previous") == 0) {
        LOGD("mpris: Previous");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "Seek") == 0) {
        LOGD("mpris: Seek");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "SetPosition") == 0) {
        LOGD("mpris: SetPosition");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "OpenUri") == 0) {
        LOGD("mpris: OpenUri");
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else {
        LOGW("mpris: unknown method %s", method_name);
		g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
											  G_DBUS_ERROR_UNKNOWN_METHOD,
											  "Unknown method");
	}
}

static GVariant *
get_property_player(G_GNUC_UNUSED GDBusConnection *connection,
					G_GNUC_UNUSED const char *sender,
					G_GNUC_UNUSED const char *object_path,
					G_GNUC_UNUSED const char *interface_name,
					const char *property_name, GError **error,
					G_GNUC_UNUSED gpointer user_data) {
	GVariant *ret;
	if (g_strcmp0(property_name, "PlaybackStatus") == 0) {
		ret = g_variant_new_string("Stopped");
	} else if (g_strcmp0(property_name, "LoopStatus") == 0) {
		ret = g_variant_new_string("None");
	} else if (g_strcmp0(property_name, "Rate") == 0) {
		ret = g_variant_new_double(1.0);
	} else if (g_strcmp0(property_name, "Shuffle") == 0) {
		ret = g_variant_new_boolean(FALSE);
	} else if (g_strcmp0(property_name, "Metadata") == 0) {
		ret = NULL;
	} else if (g_strcmp0(property_name, "Volume") == 0) {
		ret = g_variant_new_double(100);
	} else if (g_strcmp0(property_name, "Position") == 0) {
		ret = g_variant_new_int64(0);
	} else if (g_strcmp0(property_name, "MinimumRate") == 0) {
		ret = g_variant_new_double(100);
	} else if (g_strcmp0(property_name, "MaximumRate") == 0) {
		ret = g_variant_new_double(100);
	} else if (g_strcmp0(property_name, "CanGoNext") == 0) {
		ret = g_variant_new_boolean(TRUE);
	} else if (g_strcmp0(property_name, "CanGoPrevious") == 0) {
		ret = g_variant_new_boolean(TRUE);
	} else if (g_strcmp0(property_name, "CanPlay") == 0) {
		ret = g_variant_new_boolean(TRUE);
	} else if (g_strcmp0(property_name, "CanPause") == 0) {
		ret = g_variant_new_boolean(TRUE);
	} else if (g_strcmp0(property_name, "CanSeek") == 0) {
		ret = g_variant_new_boolean(FALSE);
	} else if (g_strcmp0(property_name, "CanControl") == 0) {
		ret = g_variant_new_boolean(TRUE);
	} else {
		ret = NULL;
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
					"Unknown property %s", property_name);
	}

	return ret;
}

static gboolean
set_property_player(G_GNUC_UNUSED GDBusConnection *connection,
					G_GNUC_UNUSED const char *sender,
					G_GNUC_UNUSED const char *object_path,
					G_GNUC_UNUSED const char *interface_name,
					const char *property_name, GVariant *value,
					G_GNUC_UNUSED GError **error, G_GNUC_UNUSED gpointer user_data) {
	if (g_strcmp0(property_name, "LoopStatus") == 0) {
        LOGD("mpris: setting loop status");
	} else if (g_strcmp0(property_name, "Rate") == 0) {
		double rate = g_variant_get_double(value);
        LOGD("mpris: setting rate to %f", rate);
	} else if (g_strcmp0(property_name, "Shuffle") == 0) {
		int shuffle = g_variant_get_boolean(value);
        LOGD("mpris: setting shuffle to %d", shuffle);
	} else if (g_strcmp0(property_name, "Volume") == 0) {
        double volume = g_variant_get_double(value);
        LOGD("mpris: setting volume to %f", volume);
	} else {
		g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
					"Cannot set property %s", property_name);
		return FALSE;
	}

	return TRUE;
}

static GDBusInterfaceVTable vtable_player = {
	method_call_player, get_property_player, set_property_player, {0}};

static void
on_bus_acquired(GDBusConnection *connection, G_GNUC_UNUSED const char *name,
				gpointer user_data) {
	GError *error = NULL;
	struct sc_mpris *ud = user_data;
	ud->connection = connection;

	ud->root_interface_id = g_dbus_connection_register_object(
		connection, "/org/mpris/MediaPlayer2", ud->root_interface_info,
		&vtable_root, user_data, NULL, &error);
	if (error != NULL) {
		g_printerr("%s", error->message);
	}

	ud->player_interface_id = g_dbus_connection_register_object(
		connection, "/org/mpris/MediaPlayer2", ud->player_interface_info,
		&vtable_player, user_data, NULL, &error);
	if (error != NULL) {
		g_printerr("%s", error->message);
	}
}

static void
on_name_lost(GDBusConnection *connection, G_GNUC_UNUSED const char *_name,
			 gpointer user_data) {
	if (connection) {
		struct sc_mpris *ud = user_data;
		pid_t pid = getpid();
		char *name =
			g_strdup_printf("org.mpris.MediaPlayer2.scrcpy.instance%d", pid);
		ud->bus_id = g_bus_own_name(G_BUS_TYPE_SESSION, name,
									G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL,
									NULL, &ud, NULL);
		g_free(name);
	}
}

static int
run_mpris(void *data) {
	struct sc_mpris *mpris = data;
	GMainContext *ctx;

	ctx = g_main_context_new();
	mpris->loop = g_main_loop_new(ctx, FALSE);

	g_main_context_push_thread_default(ctx);
	mpris->bus_id =
		g_bus_own_name(G_BUS_TYPE_SESSION, "org.mpris.MediaPlayer2.scrcpy",
					   G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE, on_bus_acquired,
					   NULL, on_name_lost, mpris, NULL);
	g_main_context_pop_thread_default(ctx);

	g_main_loop_run(mpris->loop);

	g_dbus_connection_unregister_object(mpris->connection,
										mpris->root_interface_id);
	g_dbus_connection_unregister_object(mpris->connection,
										mpris->player_interface_id);

	g_bus_unown_name(mpris->bus_id);
	g_main_loop_unref(mpris->loop);
	g_main_context_unref(ctx);
	g_dbus_node_info_unref(mpris->introspection_data);

	return 0;
}

bool
sc_mpris_start(struct sc_mpris *mpris) {
	LOGD("mpris: starting glib thread");

	GError *error = NULL;

	// Load introspection data and split into separate interfaces
	mpris->introspection_data =
		g_dbus_node_info_new_for_xml(introspection_xml, &error);
	if (error != NULL) {
		g_printerr("%s", error->message);
	}
	mpris->root_interface_info = g_dbus_node_info_lookup_interface(
		mpris->introspection_data, "org.mpris.MediaPlayer2");
	mpris->player_interface_info = g_dbus_node_info_lookup_interface(
		mpris->introspection_data, "org.mpris.MediaPlayer2.Player");

	mpris->changed_properties = g_hash_table_new(g_str_hash, g_str_equal);
	mpris->metadata = NULL;

	bool ok =
		sc_thread_create(&mpris->thread, run_mpris, "scrcpy-mpris", mpris);
	if (!ok) {
		LOGE("mpris: cloud not start mpris thread");
		return false;
	}
	return true;
}

void
sc_mpris_stop(struct sc_mpris *mpris) {
	LOGD("mpris: stopping glib thread");
	g_main_loop_quit(mpris->loop);
	sc_thread_join(&mpris->thread, NULL);
}
