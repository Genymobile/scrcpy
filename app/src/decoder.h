#ifndef DECODER_H
#define DECODER_H

#include "common.h"

#include "trait/packet_sink.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

struct video_buffer;

struct decoder {
    struct sc_packet_sink packet_sink; // packet sink trait

    struct video_buffer *video_buffer;

    AVCodecContext *codec_ctx;
    AVFrame *frame;
};

void
decoder_init(struct decoder *decoder, struct video_buffer *vb);

bool
decoder_open(struct decoder *decoder, const AVCodec *codec);

void
decoder_close(struct decoder *decoder);

bool
decoder_push(struct decoder *decoder, const AVPacket *packet);

#endif
