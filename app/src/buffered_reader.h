#ifndef BUFFERED_READER_H
#define BUFFERED_READER_H

#include "common.h"
#include "net.h"

struct buffered_reader {
    socket_t socket;
    void *buf;
    size_t bufsize;
    size_t offset;
    size_t len;
};

bool
buffered_reader_init(struct buffered_reader *reader, socket_t socket,
                     size_t bufsize);

void
buffered_reader_destroy(struct buffered_reader *reader);

ssize_t
buffered_reader_recv(struct buffered_reader *reader, void *buf, size_t count);

ssize_t
buffered_reader_recv_all(struct buffered_reader *reader, void *buf,
                         size_t count);

#endif
