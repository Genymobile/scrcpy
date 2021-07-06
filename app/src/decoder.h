#ifndef DECODER_H
#define DECODER_H

#include "common.h"

#include "trait/packet_sink.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

#define DECODER_MAX_SINKS 2

struct decoder {
    struct sc_packet_sink packet_sink; // packet sink trait

    struct sc_frame_sink *sinks[DECODER_MAX_SINKS];
    unsigned sink_count;

    AVCodecContext *codec_ctx;
    AVFrame *frame;
};

void
decoder_init(struct decoder *decoder);

void
decoder_add_sink(struct decoder *decoder, struct sc_frame_sink *sink);

#endif
