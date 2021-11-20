#include "adb_tunnel.h"

#include <assert.h>

#include "adb.h"
#include "util/log.h"
#include "util/net_intr.h"
#include "util/process_intr.h"

#define SC_SOCKET_NAME "scrcpy"

static bool
enable_tunnel_reverse(struct sc_intr *intr, const char *serial,
                      uint16_t local_port) {
    sc_pid pid = adb_reverse(serial, SC_SOCKET_NAME, local_port);
    return sc_process_check_success_intr(intr, pid, "adb reverse");
}

static bool
disable_tunnel_reverse(struct sc_intr *intr, const char *serial) {
    sc_pid pid = adb_reverse_remove(serial, SC_SOCKET_NAME);
    return sc_process_check_success_intr(intr, pid, "adb reverse --remove");
}

static bool
enable_tunnel_forward(struct sc_intr *intr, const char *serial,
                      uint16_t local_port) {
    sc_pid pid = adb_forward(serial, local_port, SC_SOCKET_NAME);
    return sc_process_check_success_intr(intr, pid, "adb forward");
}

static bool
disable_tunnel_forward(struct sc_intr *intr, const char *serial,
                       uint16_t local_port) {
    sc_pid pid = adb_forward_remove(serial, local_port);
    return sc_process_check_success_intr(intr, pid, "adb forward --remove");
}

static bool
listen_on_port(struct sc_intr *intr, sc_socket socket, uint16_t port) {
    return net_listen_intr(intr, socket, IPV4_LOCALHOST, port, 1);
}

static bool
enable_tunnel_reverse_any_port(struct sc_adb_tunnel *tunnel,
                               struct sc_intr *intr, const char *serial,
                               struct sc_port_range port_range) {
    uint16_t port = port_range.first;
    for (;;) {
        if (!enable_tunnel_reverse(intr, serial, port)) {
            // the command itself failed, it will fail on any port
            return false;
        }

        // At the application level, the device part is "the server" because it
        // serves video stream and control. However, at the network level, the
        // client listens and the server connects to the client. That way, the
        // client can listen before starting the server app, so there is no
        // need to try to connect until the server socket is listening on the
        // device.
        sc_socket server_socket = net_socket();
        if (server_socket != SC_SOCKET_NONE) {
            bool ok = listen_on_port(intr, server_socket, port);
            if (ok) {
                // success
                tunnel->server_socket = server_socket;
                tunnel->local_port = port;
                tunnel->enabled = true;
                return true;
            }

            net_close(server_socket);
        }

        if (sc_intr_is_interrupted(intr)) {
            // Stop immediately
            return false;
        }

        // failure, disable tunnel and try another port
        if (!disable_tunnel_reverse(intr, serial)) {
            LOGW("Could not remove reverse tunnel on port %" PRIu16, port);
        }

        // check before incrementing to avoid overflow on port 65535
        if (port < port_range.last) {
            LOGW("Could not listen on port %" PRIu16", retrying on %" PRIu16,
                 port, (uint16_t) (port + 1));
            port++;
            continue;
        }

        if (port_range.first == port_range.last) {
            LOGE("Could not listen on port %" PRIu16, port_range.first);
        } else {
            LOGE("Could not listen on any port in range %" PRIu16 ":%" PRIu16,
                 port_range.first, port_range.last);
        }
        return false;
    }
}

static bool
enable_tunnel_forward_any_port(struct sc_adb_tunnel *tunnel,
                               struct sc_intr *intr, const char *serial,
                               struct sc_port_range port_range) {
    tunnel->forward = true;

    uint16_t port = port_range.first;
    for (;;) {
        if (enable_tunnel_forward(intr, serial, port)) {
            // success
            tunnel->local_port = port;
            tunnel->enabled = true;
            return true;
        }

        if (sc_intr_is_interrupted(intr)) {
            // Stop immediately
            return false;
        }

        if (port < port_range.last) {
            LOGW("Could not forward port %" PRIu16", retrying on %" PRIu16,
                 port, (uint16_t) (port + 1));
            port++;
            continue;
        }

        if (port_range.first == port_range.last) {
            LOGE("Could not forward port %" PRIu16, port_range.first);
        } else {
            LOGE("Could not forward any port in range %" PRIu16 ":%" PRIu16,
                 port_range.first, port_range.last);
        }
        return false;
    }
}

void
sc_adb_tunnel_init(struct sc_adb_tunnel *tunnel) {
    tunnel->enabled = false;
    tunnel->forward = false;
    tunnel->server_socket = SC_SOCKET_NONE;
    tunnel->local_port = 0;
}

bool
sc_adb_tunnel_open(struct sc_adb_tunnel *tunnel, struct sc_intr *intr,
                   const char *serial, struct sc_port_range port_range,
                   bool force_adb_forward) {
    assert(!tunnel->enabled);

    if (!force_adb_forward) {
        // Attempt to use "adb reverse"
        if (enable_tunnel_reverse_any_port(tunnel, intr, serial, port_range)) {
            return true;
        }

        // if "adb reverse" does not work (e.g. over "adb connect"), it
        // fallbacks to "adb forward", so the app socket is the client

        LOGW("'adb reverse' failed, fallback to 'adb forward'");
    }

    return enable_tunnel_forward_any_port(tunnel, intr, serial, port_range);
}

bool
sc_adb_tunnel_close(struct sc_adb_tunnel *tunnel, struct sc_intr *intr,
                    const char *serial) {
    assert(tunnel->enabled);

    bool ret;
    if (tunnel->forward) {
        ret = disable_tunnel_forward(intr, serial, tunnel->local_port);
    } else {
        ret = disable_tunnel_reverse(intr, serial);

        assert(tunnel->server_socket != SC_SOCKET_NONE);
        if (!net_close(tunnel->server_socket)) {
            LOGW("Could not close server socket");
        }

        // server_socket is never used anymore
    }

    // Consider tunnel disabled even if the command failed
    tunnel->enabled = false;

    return ret;
}
