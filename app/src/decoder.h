#ifndef SC_DECODER_H
#define SC_DECODER_H

#include "common.h"

#include <libavcodec/avcodec.h>

#include "trait/frame_source.h"
#include "trait/packet_sink.h"

struct sc_decoder {
    struct sc_packet_sink packet_sink; // packet sink trait
    struct sc_frame_source frame_source; // frame source trait

    const char *name; // must be statically allocated (e.g. a string literal)

    AVCodecContext *ctx;
    AVFrame *frame;
};

// The name must be statically allocated (e.g. a string literal)
void
sc_decoder_init(struct sc_decoder *decoder, const char *name);

#endif
