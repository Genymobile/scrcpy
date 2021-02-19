#ifndef DECODER_H
#define DECODER_H

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

struct video_buffer;

struct decoder {
    struct video_buffer *video_buffer;
    struct fps_counter *fps_counter;

    AVCodecContext *codec_ctx;
};

void
decoder_init(struct decoder *decoder, struct video_buffer *vb,
             struct fps_counter *fps_counter);

bool
decoder_open(struct decoder *decoder, const AVCodec *codec);

void
decoder_close(struct decoder *decoder);

bool
decoder_push(struct decoder *decoder, const AVPacket *packet);

void
decoder_interrupt(struct decoder *decoder);

#endif
