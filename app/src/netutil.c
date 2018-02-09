#include "netutil.h"

#include <SDL2/SDL_net.h>

// contrary to SDLNet_TCP_Send and SDLNet_TCP_Recv, SDLNet_TCP_Accept is non-blocking
// so we need to block before calling it
TCPsocket server_socket_accept(TCPsocket server_socket, Uint32 timeout_ms) {
    SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
    if (!set) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not allocate socket set");
        return NULL;
    }

    if (SDLNet_TCP_AddSocket(set, server_socket) == -1) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not add socket to set");
        SDLNet_FreeSocketSet(set);
        return NULL;
    }

    if (SDLNet_CheckSockets(set, timeout_ms) != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "No connection to accept");
        SDLNet_FreeSocketSet(set);
        return NULL;
    }

    SDLNet_FreeSocketSet(set);

    return SDLNet_TCP_Accept(server_socket);
}
