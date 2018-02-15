#ifndef NET_H
#define NET_H

#include <SDL2/SDL_platform.h>
#include <SDL2/SDL_stdinc.h>

#ifdef __WINDOWS__
# include <winsock2.h>
  typedef SIZE_T size_t;
  typedef SSIZE_T ssize_t;
  typedef SOCKET socket_t;
#else
# define INVALID_SOCKET -1
  typedef int socket_t;
#endif

SDL_bool net_init(void);
void net_cleanup(void);

socket_t net_listen(Uint32 addr, Uint16 port, int backlog);
socket_t net_accept(socket_t server_socket);
ssize_t net_recv(socket_t socket, void *buf, size_t len);
ssize_t net_send(socket_t socket, void *buf, size_t len);
void net_close(socket_t socket);

#endif
