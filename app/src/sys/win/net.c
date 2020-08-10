#include "util/net.h"

#include "config.h"
#include "util/log.h"

bool
net_init(void) {
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
    if (res < 0) {
        LOGC("WSAStartup failed with error %d", res);
        return false;
    }
    return true;
}

void
net_cleanup(void) {
    WSACleanup();
}

bool
net_close(socket_t socket) {
    return !closesocket(socket);
}
