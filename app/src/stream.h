#ifndef STREAM_H
#define STREAM_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavformat/avformat.h>

#include "trait/packet_sink.h"
#include "util/net.h"
#include "util/thread.h"

#define STREAM_MAX_SINKS 2

struct stream {
    sc_socket socket;
    sc_thread thread;

    struct sc_packet_sink *sinks[STREAM_MAX_SINKS];
    unsigned sink_count;

    AVCodecContext *codec_ctx;
    AVCodecParserContext *parser;
    // successive packets may need to be concatenated, until a non-config
    // packet is available
    AVPacket *pending;

    const struct stream_callbacks *cbs;
    void *cbs_userdata;
};

struct stream_callbacks {
    void (*on_eos)(struct stream *stream, void *userdata);
};

void
stream_init(struct stream *stream, sc_socket socket,
            const struct stream_callbacks *cbs, void *cbs_userdata);

void
stream_add_sink(struct stream *stream, struct sc_packet_sink *sink);

bool
stream_start(struct stream *stream);

void
stream_join(struct stream *stream);

#endif
