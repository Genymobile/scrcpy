#include "buffered_reader.h"

#include <SDL2/SDL_assert.h>
#include "log.h"

bool
buffered_reader_init(struct buffered_reader *reader, socket_t socket,
                     size_t bufsize) {
    reader->buf = SDL_malloc(bufsize);
    if (!reader->buf) {
        LOGC("Could not allocate buffer");
        return false;
    }

    reader->socket = socket;
    reader->bufsize = bufsize;
    reader->offset = 0;
    reader->len = 0;

    return true;
}

void
buffered_reader_destroy(struct buffered_reader *reader) {
    SDL_free(reader->buf);
}

static ssize_t
buffered_reader_fill(struct buffered_reader *reader) {
    SDL_assert(!reader->len);
    ssize_t r = net_recv(reader->socket, reader->buf, reader->bufsize);
    if (r > 0) {
        reader->offset = 0;
        reader->len = r;
    }
    return r;
}

ssize_t
buffered_reader_recv(struct buffered_reader *reader, void *buf, size_t count) {
    if (!reader->len) {
        // read from the socket
        ssize_t r = buffered_reader_fill(reader);
        if (r <= 0) {
            return r;
        }
    }

    size_t r = count < reader->len ? count : reader->len;
    memcpy(buf, reader->buf + reader->offset, r);
    reader->offset += r;
    reader->len -= r;
    return r;
}

ssize_t
buffered_reader_recv_all(struct buffered_reader *reader, void *buf,
                         size_t count) {
    size_t done = 0;
    while (done < count) {
        ssize_t r = buffered_reader_recv(reader, buf, count - done);
        if (r <= 0) {
            // if there was some data, return them immediately
            return done ? done : r;
        }

        done += r;
        buf += r;
    }

    return done;
}
