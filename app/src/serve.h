#ifndef SERVE_H
#define SERVE_H

#include <stdbool.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL_atomic.h>
#include <SDL2/SDL_thread.h>

#include "config.h"
#include "util/net.h"

struct serve {
    socket_t socket;
    char *protocol;
    uint32_t ip;
    uint16_t port;
};

void
serve_init(struct serve* serve, char* protocol, uint32_t ip, uint16_t port);

bool
serve_start(struct serve* serve);

bool
serve_push(struct serve* serve, const AVPacket *packet);
#endif