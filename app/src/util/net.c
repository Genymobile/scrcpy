#include "net.h"

#include <assert.h>
#include <stdio.h>
#include <SDL2/SDL_platform.h>

#include "log.h"

#ifdef __WINDOWS__
  typedef int socklen_t;
  typedef SOCKET sc_raw_socket;
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# define SOCKET_ERROR -1
  typedef struct sockaddr_in SOCKADDR_IN;
  typedef struct sockaddr SOCKADDR;
  typedef struct in_addr IN_ADDR;
  typedef int sc_raw_socket;
#endif

bool
net_init(void) {
#ifdef __WINDOWS__
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
    if (res < 0) {
        LOGC("WSAStartup failed with error %d", res);
        return false;
    }
#endif
    return true;
}

void
net_cleanup(void) {
#ifdef __WINDOWS__
    WSACleanup();
#endif
}

static inline sc_socket
wrap(sc_raw_socket sock) {
#ifdef __WINDOWS__
    if (sock == INVALID_SOCKET) {
        return SC_SOCKET_NONE;
    }

    struct sc_socket_windows *socket = malloc(sizeof(*socket));
    if (!socket) {
        closesocket(sock);
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
#ifdef __WINDOWS__
    if (socket == SC_SOCKET_NONE) {
        return INVALID_SOCKET;
    }

    return socket->socket;
#else
    return socket;
#endif
}

static void
net_perror(const char *s) {
#ifdef _WIN32
    int error = WSAGetLastError();
    char *wsa_message;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (char *) &wsa_message, 0, NULL);
    // no explicit '\n', wsa_message already contains a trailing '\n'
    fprintf(stderr, "%s: [%d] %s", s, error, wsa_message);
    LocalFree(wsa_message);
#else
    perror(s);
#endif
}

sc_socket
net_socket(void) {
    sc_raw_socket raw_sock = socket(AF_INET, SOCK_STREAM, 0);
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
net_listen(sc_socket socket, uint32_t addr, uint16_t port, int backlog) {
    sc_raw_socket raw_sock = unwrap(socket);

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
    sc_raw_socket raw_sock =
        accept(raw_server_socket, (SOCKADDR *) &csin, &sinsize);

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

#ifdef __WINDOWS__
    if (!atomic_flag_test_and_set(&socket->closed)) {
        return !closesocket(raw_sock);
    }
    return true;
#else
    return !shutdown(raw_sock, SHUT_RDWR);
#endif
}

#include <errno.h>
bool
net_close(sc_socket socket) {
    sc_raw_socket raw_sock = unwrap(socket);

#ifdef __WINDOWS__
    bool ret = true;
    if (!atomic_flag_test_and_set(&socket->closed)) {
        ret = !closesocket(raw_sock);
    }
    free(socket);
    return ret;
#else
    return !close(raw_sock);
#endif
}
