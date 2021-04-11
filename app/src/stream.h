#ifndef STREAM_H
#define STREAM_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL_atomic.h>

#include "trait/packet_sink.h"
#include "util/net.h"
#include "util/thread.h"

#define STREAM_MAX_SINKS 2

struct stream {
    socket_t socket;
    sc_thread thread;

    struct sc_packet_sink *sinks[STREAM_MAX_SINKS];
    unsigned sink_count;

    AVCodecContext *codec_ctx;
    AVCodecParserContext *parser;
    // successive packets may need to be concatenated, until a non-config
    // packet is available
    bool has_pending;
    AVPacket pending;
};

void
stream_init(struct stream *stream, socket_t socket);

void
stream_add_sink(struct stream *stream, struct sc_packet_sink *sink);

bool
stream_start(struct stream *stream);

void
stream_join(struct stream *stream);

#endif
