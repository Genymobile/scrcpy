#ifndef DECODER_H
#define DECODER_H

#include <libavformat/avformat.h>
#include <SDL2/SDL_stdinc.h>

struct video_buffer;

struct decoder {
    struct video_buffer *video_buffer;
    AVCodecContext *codec_ctx;
};

void
decoder_init(struct decoder *decoder, struct video_buffer *vb);

SDL_bool
decoder_open(struct decoder *decoder, AVCodec *codec);

void
decoder_close(struct decoder *decoder);

SDL_bool
decoder_push(struct decoder *decoder, AVPacket *packet);

void
decoder_interrupt(struct decoder *decoder);

#endif
