#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL_atomic.h>
#include <SDL2/SDL_thread.h>

#include "config.h"
#include "util/net.h"

struct video_buffer;

struct stream {
    socket_t socket;
    struct video_buffer *video_buffer;
    SDL_Thread *thread;
    struct decoder *decoder;
    struct recorder *recorder;
    AVCodecContext *codec_ctx;
    AVCodecParserContext *parser;
    // successive packets may need to be concatenated, until a non-config
    // packet is available
    bool has_pending;
    AVPacket pending;
};

void
stream_init(struct stream *stream, socket_t socket,
            struct decoder *decoder, struct recorder *recorder);

bool
stream_start(struct stream *stream);

void
stream_stop(struct stream *stream);

void
stream_join(struct stream *stream);

#endif
