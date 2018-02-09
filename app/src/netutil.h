#ifndef NETUTIL_H
#define NETUTIL_H

#include <SDL2/SDL_net.h>

// blocking accept on the server socket
TCPsocket server_socket_accept(TCPsocket server_socket, Uint32 timeout_ms);

#endif
