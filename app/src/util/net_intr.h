#ifndef SC_NET_INTR_H
#define SC_NET_INTR_H

#include "common.h"

#include "intr.h"
#include "net.h"

bool
net_connect_intr(struct sc_intr *intr, sc_socket socket, uint32_t addr,
                 uint16_t port);

bool
net_listen_intr(struct sc_intr *intr, sc_socket server_socket, uint32_t addr,
                uint16_t port, int backlog);

sc_socket
net_accept_intr(struct sc_intr *intr, sc_socket server_socket);

ssize_t
net_recv_intr(struct sc_intr *intr, sc_socket socket, void *buf, size_t len);

ssize_t
net_recv_all_intr(struct sc_intr *intr, sc_socket socket, void *buf,
                  size_t len);

ssize_t
net_send_intr(struct sc_intr *intr, sc_socket socket, const void *buf,
              size_t len);

ssize_t
net_send_all_intr(struct sc_intr *intr, sc_socket socket, const void *buf,
                  size_t len);

#endif
