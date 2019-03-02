#include "net.h"

#include <unistd.h>

bool
net_init(void) {
    // do nothing
    return true;
}

void
net_cleanup(void) {
    // do nothing
}

bool
net_close(socket_t socket) {
    return !close(socket);
}
