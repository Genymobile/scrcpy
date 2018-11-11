#ifndef RECORDER_H
#define RECORDER_H

#include <libavformat/avformat.h>
#include <SDL2/SDL_stdinc.h>

#include "common.h"

struct recorder {
    char *filename;
    AVFormatContext *ctx;
    struct size declared_frame_size;
};

SDL_bool recorder_init(struct recorder *recoder, const char *filename,
                       struct size declared_frame_size);
void recorder_destroy(struct recorder *recorder);

SDL_bool recorder_open(struct recorder *recorder, AVCodec *input_codec);
void recorder_close(struct recorder *recorder);

SDL_bool recorder_write(struct recorder *recorder, AVPacket *packet);

#endif
