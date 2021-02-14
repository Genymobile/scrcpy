#ifndef DECODER_H
#define DECODER_H

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

struct video_buffer;

struct decoder {
    struct video_buffer *video_buffer;
    AVCodecContext *codec_ctx;

    const struct decoder_callbacks *cbs;
    void *cbs_userdata;
};

struct decoder_callbacks {
     // Called when a new frame can be consumed by
     // video_buffer_take_rendering_frame(decoder->video_buffer).
    void (*on_new_frame)(struct decoder *decoder, void *userdata);
};

void
decoder_init(struct decoder *decoder, struct video_buffer *vb,
             const struct decoder_callbacks *cbs, void *cbs_userdata);

bool
decoder_open(struct decoder *decoder, const AVCodec *codec);

void
decoder_close(struct decoder *decoder);

bool
decoder_push(struct decoder *decoder, const AVPacket *packet);

void
decoder_interrupt(struct decoder *decoder);

#endif
