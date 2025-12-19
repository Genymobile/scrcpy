#include "control_forwarder.h"

#include <assert.h>
#include <string.h>

#include "control_msg.h"
#include "util/log.h"

#define SC_CONTROL_MSG_MAX_SIZE 256

static int
run_control_forwarder(void *data) {
    struct sc_control_forwarder *forwarder = data;
    
    // Create server socket
    forwarder->server_socket = net_socket();
    if (forwarder->server_socket == SC_SOCKET_NONE) {
        LOGE("Control forwarder: could not create server socket");
        return -1;
    }
    
    // Bind and listen
    if (!net_listen(forwarder->server_socket, IPV4_LOCALHOST, forwarder->port, 1)) {
        LOGE("Control forwarder: could not listen on port %u", forwarder->port);
        net_close(forwarder->server_socket);
        return -1;
    }
    
    LOGI("Control forwarder: listening on port %u", forwarder->port);
    
    while (!forwarder->stopped) {
        // Accept a client connection (blocking)
        forwarder->client_socket = net_accept(forwarder->server_socket);
        if (forwarder->client_socket == SC_SOCKET_NONE) {
            if (forwarder->stopped) {
                break;
            }
            LOGW("Control forwarder: failed to accept client connection");
            continue;
        }
        
        LOGI("Control forwarder: client connected");
        
        // Forward control messages from TCP client to scrcpy control socket
        bool client_connected = true;
        uint8_t buffer[SC_CONTROL_MSG_MAX_SIZE];
        
        while (client_connected && !forwarder->stopped) {
            // Receive control message from TCP client
            // Control messages are variable length, so we need to read carefully
            ssize_t r = net_recv(forwarder->client_socket, buffer, sizeof(buffer));
            
            if (r <= 0) {
                // Client disconnected or error
                if (r < 0) {
                    LOGW("Control forwarder: receive error");
                }
                LOGI("Control forwarder: client disconnected");
                client_connected = false;
                break;
            }
            
            // Forward the received bytes directly to the control socket
            sc_socket control_socket = forwarder->controller->control_socket;
            ssize_t w = net_send_all(control_socket, buffer, r);
            if (w != r) {
                LOGW("Control forwarder: failed to forward control message");
                client_connected = false;
                break;
            }
        }
        
        // Client disconnected
        if (forwarder->client_socket != SC_SOCKET_NONE) {
            net_close(forwarder->client_socket);
            forwarder->client_socket = SC_SOCKET_NONE;
        }
    }
    
    // Cleanup
    if (forwarder->server_socket != SC_SOCKET_NONE) {
        net_close(forwarder->server_socket);
        forwarder->server_socket = SC_SOCKET_NONE;
    }
    
    LOGD("Control forwarder thread ended");
    return 0;
}

bool
sc_control_forwarder_init(struct sc_control_forwarder *forwarder, uint16_t port) {
    forwarder->port = port;
    forwarder->server_socket = SC_SOCKET_NONE;
    forwarder->client_socket = SC_SOCKET_NONE;
    forwarder->stopped = false;
    forwarder->controller = NULL;
    
    if (!sc_mutex_init(&forwarder->mutex)) {
        return false;
    }
    
    return true;
}

bool
sc_control_forwarder_start(struct sc_control_forwarder *forwarder,
                           struct sc_controller *controller) {
    assert(controller);
    forwarder->controller = controller;
    
    if (!sc_thread_create(&forwarder->thread, run_control_forwarder, "ctrl-fwd",
                         forwarder)) {
        LOGE("Control forwarder: could not create thread");
        return false;
    }
    
    return true;
}

void
sc_control_forwarder_stop(struct sc_control_forwarder *forwarder) {
    sc_mutex_lock(&forwarder->mutex);
    forwarder->stopped = true;
    sc_mutex_unlock(&forwarder->mutex);
    
    // Close server socket to unblock accept()
    if (forwarder->server_socket != SC_SOCKET_NONE) {
        net_close(forwarder->server_socket);
    }
    
    // Close client socket to unblock recv()
    if (forwarder->client_socket != SC_SOCKET_NONE) {
        net_close(forwarder->client_socket);
    }
}

void
sc_control_forwarder_join(struct sc_control_forwarder *forwarder) {
    sc_thread_join(&forwarder->thread, NULL);
}

void
sc_control_forwarder_destroy(struct sc_control_forwarder *forwarder) {
    sc_mutex_destroy(&forwarder->mutex);
}
