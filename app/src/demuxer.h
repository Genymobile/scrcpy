#ifndef SC_DEMUXER_H
#define SC_DEMUXER_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "trait/packet_sink.h"
#include "util/net.h"
#include "util/thread.h"

#define SC_DEMUXER_MAX_SINKS 2

struct sc_demuxer {
    sc_socket socket;
    sc_thread thread;

    struct sc_packet_sink *sinks[SC_DEMUXER_MAX_SINKS];
    unsigned sink_count;

    AVCodecContext *codec_ctx;
    AVCodecParserContext *parser;
    // successive packets may need to be concatenated, until a non-config
    // packet is available
    AVPacket *pending;

    const struct sc_demuxer_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_demuxer_callbacks {
    void (*on_eos)(struct sc_demuxer *demuxer, void *userdata);
};

void
sc_demuxer_init(struct sc_demuxer *demuxer, sc_socket socket,
                const struct sc_demuxer_callbacks *cbs, void *cbs_userdata);

void
sc_demuxer_add_sink(struct sc_demuxer *demuxer, struct sc_packet_sink *sink);

bool
sc_demuxer_start(struct sc_demuxer *demuxer);

void
sc_demuxer_join(struct sc_demuxer *demuxer);

#endif
