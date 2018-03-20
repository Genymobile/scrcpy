#include "net.h"

#include "log.h"

SDL_bool net_init(void) {
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
    if (res < 0) {
        LOGC("WSAStartup failed with error %d", res);
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

void net_cleanup(void) {
    WSACleanup();
}

SDL_bool net_close(socket_t socket) {
    return !closesocket(socket);
}
