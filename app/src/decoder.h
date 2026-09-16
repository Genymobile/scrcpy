#ifndef SC_DECODER_H
#define SC_DECODER_H

#include "common.h"

#include <libavcodec/avcodec.h>

#include "coords.h"
#include "trait/frame_source.h"
#include "trait/packet_sink.h"

struct sc_decoder {
    struct sc_packet_sink packet_sink; // packet sink trait
    struct sc_frame_source frame_source; // frame source trait

    const char *name; // must be statically allocated (e.g. a string literal)
    bool copy_opaque;

    AVCodecContext *ctx;
    AVFrame *frame;

    struct sc_stream_session session; // only initialized for video stream
    struct sc_size frame_size;
};

// The name must be statically allocated (e.g. a string literal)
//
// copy_opaque: Set to true to forward the AVPacket.opaque_ref to the AVFrame
void
sc_decoder_init(struct sc_decoder *decoder, const char *name, bool copy_opaque);

#endif
