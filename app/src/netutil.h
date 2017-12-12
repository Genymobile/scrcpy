#ifndef NETUTIL_H
#define NETUTIL_H

#include <SDL2/SDL_net.h>

TCPsocket blocking_accept(TCPsocket server_socket);

#endif
