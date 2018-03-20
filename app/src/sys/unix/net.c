#include "net.h"

# include <unistd.h>

SDL_bool net_init(void) {
    // do nothing
    return SDL_TRUE;
}

void net_cleanup(void) {
    // do nothing
}

SDL_bool net_close(socket_t socket) {
    return !close(socket);
}
