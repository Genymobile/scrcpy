#include "serve.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>

#include "config.h"
#include "events.h"
#include <sys/types.h>
#include "util/log.h"
#include "util/net.h"

# define SOCKET_ERROR (-1)

void
serve_init(struct serve *serve, char *protocol, uint32_t ip, uint16_t port) {
    serve->protocol = protocol;
    serve->ip = ip;
    serve->port = port;
    serve->isServeReady = false;
}

bool
serve_start(struct serve *serve) {
    LOGD("Starting serve thread");

    socket_t Listensocket;
    socket_t ClientSocket;

    Listensocket = net_listen(serve->ip, serve->port, 1);
    if (Listensocket == INVALID_SOCKET) {
        LOGI("Listen socket error");
        net_close(Listensocket);
        return 0;
    }

    ClientSocket = net_accept(Listensocket);
    if (ClientSocket == INVALID_SOCKET) {
        LOGI("Client socket error");
        net_close(Listensocket);
        return 0;
    }

    LOGI("Client found");
    net_close(Listensocket);

    serve->socket = ClientSocket;

    serve->isServeReady = true;
    LOGI("Serve is ready to forward the stream");

    return true;
}

bool
serve_push(struct serve *serve, const AVPacket *packet) {
    if (serve->isServeReady)
    {
        if (net_send(serve->socket, packet->data, packet->size) == SOCKET_ERROR) {
            LOGI("Client lost");
            serve->isServeReady = false;
            net_close(serve->socket);
            return false;
        }
        return true;
    }
    return false;
}

void
serve_stop(struct serve *serve) {
    net_close(serve->socket);
}