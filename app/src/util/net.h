#ifndef SC_NET_H
#define SC_NET_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef _WIN32
# include <winsock2.h>
  typedef SOCKET sc_raw_socket;
# define SC_RAW_SOCKET_NONE INVALID_SOCKET
#else // not _WIN32
  typedef int sc_raw_socket;
# define SC_RAW_SOCKET_NONE -1
#endif

#if defined(_WIN32) || defined(__APPLE__)
// On Windows and macOS, shutdown() does not interrupt accept() or read()
// calls, so net_interrupt() must call close() instead, and net_close() must
// behave accordingly.
// This causes a small race condition (once the socket is closed, its
// handle becomes invalid and may in theory be reassigned before another
// thread calls accept() or read()), but it is deemed acceptable as a
// workaround.
# define SC_SOCKET_CLOSE_ON_INTERRUPT
#endif

#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
# include <stdatomic.h>
# define SC_SOCKET_NONE NULL
  typedef struct sc_socket_wrapper {
      sc_raw_socket socket;
      atomic_flag closed;
  } *sc_socket;
#else
# define SC_SOCKET_NONE -1
  typedef sc_raw_socket sc_socket;
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

// Disable Nagle's algorithm (if tcp_nodelay is true)
bool
net_set_tcp_nodelay(sc_socket socket, bool tcp_nodelay);

/**
 * Parse `ip` "xxx.xxx.xxx.xxx" to an IPv4 host representation
 */
bool
net_parse_ipv4(const char *ip, uint32_t *ipv4);

#endif
