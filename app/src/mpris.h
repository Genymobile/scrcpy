#ifndef SC_MPRIS_H
#define SC_MPRIS_H

#include "common.h"
#include "util/thread.h"
#include <sys/wait.h>
#include <gio/gio.h>

struct sc_mpris {
	sc_thread thread;
	GMainLoop *loop;
	gint bus_id;
	GDBusNodeInfo *introspection_data;
	GDBusConnection *connection;
	GDBusInterfaceInfo *root_interface_info;
	GDBusInterfaceInfo *player_interface_info;
	guint root_interface_id;
	guint player_interface_id;
	const char *status;
	const char *loop_status;
	GHashTable *changed_properties;
	GVariant *metadata;
};

bool sc_mpris_start(struct sc_mpris *mpris);

void sc_mpris_stop(struct sc_mpris *mpris);

#endif
