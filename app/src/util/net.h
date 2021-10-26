#ifndef NET_H
#define NET_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL_platform.h>

#ifdef __WINDOWS__

# include <winsock2.h>
# define SHUT_RD SD_RECEIVE
# define SHUT_WR SD_SEND
# define SHUT_RDWR SD_BOTH
# define SC_INVALID_SOCKET INVALID_SOCKET
  typedef SOCKET sc_socket;

#else // not __WINDOWS__

# include <sys/socket.h>
# define SC_INVALID_SOCKET -1
  typedef int sc_socket;
#endif

bool
net_init(void);

void
net_cleanup(void);

sc_socket
net_connect(uint32_t addr, uint16_t port);

sc_socket
net_listen(uint32_t addr, uint16_t port, int backlog);

sc_socket
net_accept(sc_socket server_socket);

// the _all versions wait/retry until len bytes have been written/read
ssize_t
net_recv(sc_socket socket, void *buf, size_t len);

ssize_t
net_recv_all(sc_socket socket, void *buf, size_t len);

ssize_t
net_send(sc_socket socket, const void *buf, size_t len);

ssize_t
net_send_all(sc_socket socket, const void *buf, size_t len);

// how is SHUT_RD (read), SHUT_WR (write) or SHUT_RDWR (both)
bool
net_shutdown(sc_socket socket, int how);

bool
net_close(sc_socket socket);

#endif
