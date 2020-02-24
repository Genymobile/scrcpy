#include "serve.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>

#include "config.h"
#include "events.h"
#include "util/log.h"
#include "util/net.h"

# define SOCKET_ERROR -1

void
serve_init(struct serve* serve, char *protocol, uint32_t ip, uint16_t port) {
	serve->protocol = protocol;
	serve->ip = ip;
	serve->port = port;
}

//static int
//run_serve(void *data) {
//	struct serve* serve = data;
//
//	socket_t Listensocket;
//	socket_t ClientSocket;
//
//	Listensocket = net_listen(serve->ip, serve->port, 1);
//	if (Listensocket == INVALID_SOCKET) {
//		LOGI("Listen Error");
//		net_close(Listensocket);
//		return 0;
//	}
//
//	for (;;) {
//		ClientSocket = net_accept(Listensocket);
//		if (ClientSocket == INVALID_SOCKET) {
//			LOGI("Client Error");
//			net_close(Listensocket);
//			return 0;
//		}
//		LOGI("Client found");
//
//		net_close(Listensocket);
//
//		serve->socket = ClientSocket;
//
//		if (serve->stopped)
//		{
//			break;
//		}
//	}
//
//	LOGD("Serve thread ended");
//	return 0;
//}

bool
serve_start(struct serve* serve) {
	LOGD("Starting serve thread");

	socket_t Listensocket;
	socket_t ClientSocket;

	Listensocket = net_listen(serve->ip, serve->port, 1);
	if (Listensocket == INVALID_SOCKET) {
		LOGI("Listen Error");
		net_close(Listensocket);
		return 0;
	}

	ClientSocket = net_accept(Listensocket);
	if (ClientSocket == INVALID_SOCKET) {
		LOGI("Client Error");
		net_close(Listensocket);
		return 0;
	}
	LOGI("Client found");

	net_close(Listensocket);

	serve->socket = ClientSocket;

	/*serve->thread = SDL_CreateThread(run_serve, "serve", serve);
	if (!serve->thread) {
		LOGC("Could not start stream thread");
		return false;
	}*/
	return true;
}

bool
serve_push(struct serve* serve, const AVPacket packet) {
	if (net_send(serve->socket, packet.data, packet.size) == SOCKET_ERROR)
	{
		LOGI("Client lost");
		net_close(serve->socket);
		return false;
	}
	return true;
}
