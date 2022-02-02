#ifndef SC_DECODER_H
#define SC_DECODER_H

#include "common.h"

#include "trait/packet_sink.h"

#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define SC_DECODER_MAX_SINKS 2

struct sc_decoder {
    struct sc_packet_sink packet_sink; // packet sink trait

    struct sc_frame_sink *sinks[SC_DECODER_MAX_SINKS];
    unsigned sink_count;

    AVCodecContext *codec_ctx;
    AVFrame *frame;
};

void
sc_decoder_init(struct sc_decoder *decoder);

void
sc_decoder_add_sink(struct sc_decoder *decoder, struct sc_frame_sink *sink);

#endif
