#include "net.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
# include <ws2tcpip.h>
  typedef int socklen_t;
#else
# include <arpa/inet.h>
# include <fcntl.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/types.h>
# define SOCKET_ERROR -1
  typedef struct sockaddr_in SOCKADDR_IN;
  typedef struct sockaddr SOCKADDR;
  typedef struct in_addr IN_ADDR;
#endif

#include "util/log.h"

bool
net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(1, 1), &wsa);
    if (res) {
        LOGE("WSAStartup failed with error %d", res);
        return false;
    }
#endif
    return true;
}

void
net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static inline bool
sc_raw_socket_close(sc_raw_socket raw_sock) {
#ifndef _WIN32
    return !close(raw_sock);
#else
    return !closesocket(raw_sock);
#endif
}

static inline sc_socket
wrap(sc_raw_socket sock) {
#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
    if (sock == SC_RAW_SOCKET_NONE) {
        return SC_SOCKET_NONE;
    }

    struct sc_socket_wrapper *socket = malloc(sizeof(*socket));
    if (!socket) {
        LOG_OOM();
        sc_raw_socket_close(sock);
        return SC_SOCKET_NONE;
    }

    socket->socket = sock;
    socket->closed = (atomic_flag) ATOMIC_FLAG_INIT;

    return socket;
#else
    return sock;
#endif
}

static inline sc_raw_socket
unwrap(sc_socket socket) {
#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
    if (socket == SC_SOCKET_NONE) {
        return SC_RAW_SOCKET_NONE;
    }

    return socket->socket;
#else
    return socket;
#endif
}

#ifndef HAVE_SOCK_CLOEXEC
// If SOCK_CLOEXEC does not exist, the flag must be set manually once the
// socket is created
static bool
set_cloexec_flag(sc_raw_socket raw_sock) {
#ifndef _WIN32
    if (fcntl(raw_sock, F_SETFD, FD_CLOEXEC) == -1) {
        perror("fcntl F_SETFD");
        return false;
    }
#else
    if (!SetHandleInformation((HANDLE) raw_sock, HANDLE_FLAG_INHERIT, 0)) {
        LOGE("SetHandleInformation socket failed");
        return false;
    }
#endif
    return true;
}
#endif

static void
net_perror(const char *s) {
#ifdef _WIN32
    sc_log_windows_error(s, WSAGetLastError());
#else
    perror(s);
#endif
}

sc_socket
net_socket(void) {
#ifdef HAVE_SOCK_CLOEXEC
    sc_raw_socket raw_sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
    sc_raw_socket raw_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (raw_sock != SC_RAW_SOCKET_NONE && !set_cloexec_flag(raw_sock)) {
        sc_raw_socket_close(raw_sock);
        return SC_SOCKET_NONE;
    }
#endif

    sc_socket sock = wrap(raw_sock);
    if (sock == SC_SOCKET_NONE) {
        net_perror("socket");
    }
    return sock;
}

bool
net_connect(sc_socket socket, uint32_t addr, uint16_t port) {
    sc_raw_socket raw_sock = unwrap(socket);

    SOCKADDR_IN sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(addr);
    sin.sin_port = htons(port);

    if (connect(raw_sock, (SOCKADDR *) &sin, sizeof(sin)) == SOCKET_ERROR) {
        net_perror("connect");
        return false;
    }

    return true;
}

bool
net_listen(sc_socket server_socket, uint32_t addr, uint16_t port, int backlog) {
    sc_raw_socket raw_sock = unwrap(server_socket);

    int reuse = 1;
    if (setsockopt(raw_sock, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse,
                   sizeof(reuse)) == -1) {
        net_perror("setsockopt(SO_REUSEADDR)");
    }

    SOCKADDR_IN sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(addr); // htonl() harmless on INADDR_ANY
    sin.sin_port = htons(port);

    if (bind(raw_sock, (SOCKADDR *) &sin, sizeof(sin)) == SOCKET_ERROR) {
        net_perror("bind");
        return false;
    }

    if (listen(raw_sock, backlog) == SOCKET_ERROR) {
        net_perror("listen");
        return false;
    }

    return true;
}

sc_socket
net_accept(sc_socket server_socket) {
    sc_raw_socket raw_server_socket = unwrap(server_socket);

    SOCKADDR_IN csin;
    socklen_t sinsize = sizeof(csin);

#ifdef HAVE_SOCK_CLOEXEC
    sc_raw_socket raw_sock =
        accept4(raw_server_socket, (SOCKADDR *) &csin, &sinsize, SOCK_CLOEXEC);
#else
    sc_raw_socket raw_sock =
        accept(raw_server_socket, (SOCKADDR *) &csin, &sinsize);
    if (raw_sock != SC_RAW_SOCKET_NONE && !set_cloexec_flag(raw_sock)) {
        sc_raw_socket_close(raw_sock);
        return SC_SOCKET_NONE;
    }
#endif

    return wrap(raw_sock);
}

ssize_t
net_recv(sc_socket socket, void *buf, size_t len) {
    sc_raw_socket raw_sock = unwrap(socket);
    return recv(raw_sock, buf, len, 0);
}

ssize_t
net_recv_all(sc_socket socket, void *buf, size_t len) {
    sc_raw_socket raw_sock = unwrap(socket);
    return recv(raw_sock, buf, len, MSG_WAITALL);
}

ssize_t
net_send(sc_socket socket, const void *buf, size_t len) {
    sc_raw_socket raw_sock = unwrap(socket);
    return send(raw_sock, buf, len, 0);
}

ssize_t
net_send_all(sc_socket socket, const void *buf, size_t len) {
    size_t copied = 0;
    while (len > 0) {
        ssize_t w = net_send(socket, buf, len);
        if (w == -1) {
            return copied ? (ssize_t) copied : -1;
        }
        len -= w;
        buf = (char *) buf + w;
        copied += w;
    }
    return copied;
}

bool
net_interrupt(sc_socket socket) {
    assert(socket != SC_SOCKET_NONE);

    sc_raw_socket raw_sock = unwrap(socket);

#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
    if (!atomic_flag_test_and_set(&socket->closed)) {
        return sc_raw_socket_close(raw_sock);
    }
    return true;
#else
    return !shutdown(raw_sock, SHUT_RDWR);
#endif
}

bool
net_close(sc_socket socket) {
    sc_raw_socket raw_sock = unwrap(socket);

#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
    bool ret = true;
    if (!atomic_flag_test_and_set(&socket->closed)) {
        ret = sc_raw_socket_close(raw_sock);
    }
    free(socket);
    return ret;
#else
    return sc_raw_socket_close(raw_sock);
#endif
}

bool
net_set_tcp_nodelay(sc_socket socket, bool tcp_nodelay) {
    sc_raw_socket raw_sock = unwrap(socket);

    int value = tcp_nodelay ? 1 : 0;
    int ret = setsockopt(raw_sock, IPPROTO_TCP, TCP_NODELAY,
                         (const void *) &value, sizeof(value));
    if (ret == -1) {
        net_perror("setsockopt(TCP_NODELAY)");
        return false;
    }

    assert(ret == 0);
    return true;
}

bool
net_parse_ipv4(const char *s, uint32_t *ipv4) {
    struct in_addr addr;
    if (!inet_pton(AF_INET, s, &addr)) {
        LOGE("Invalid IPv4 address: %s", s);
        return false;
    }

    *ipv4 = ntohl(addr.s_addr);
    return true;
}
