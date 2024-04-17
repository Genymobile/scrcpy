#ifndef SC_NET_H
#define SC_NET_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32

# include <winsock2.h>
# include <stdatomic.h>
# define SC_SOCKET_NONE NULL
  typedef struct sc_socket_windows {
      SOCKET socket;
      atomic_flag closed;
  } *sc_socket;

#else // not _WIN32

# include <sys/socket.h>
# define SC_SOCKET_NONE -1
  typedef int sc_socket;

#endif

#define IPV4_LOCALHOST 0x7F000001

bool
net_init(void);

void
net_cleanup(void);

sc_socket
net_socket(void);

bool
net_connect(sc_socket socket, uint32_t addr, uint16_t port);

bool
net_listen(sc_socket server_socket, uint32_t addr, uint16_t port, int backlog);

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

// Shutdown the socket (or close on Windows) so that any blocking send() or
// recv() are interrupted.
bool
net_interrupt(sc_socket socket);

// Close the socket.
// A socket must always be closed, even if net_interrupt() has been called.
bool
net_close(sc_socket socket);

/**
 * Parse `ip` "xxx.xxx.xxx.xxx" to an IPv4 host representation
 */
bool
net_parse_ipv4(const char *ip, uint32_t *ipv4);

#endif
