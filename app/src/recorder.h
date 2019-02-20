#ifndef RECORDER_H
#define RECORDER_H

#include <libavformat/avformat.h>
#include <SDL2/SDL_stdinc.h>

#include "common.h"

enum recorder_format {
    RECORDER_FORMAT_MP4 = 1,
    RECORDER_FORMAT_MKV,
};

struct recorder {
    char *filename;
    enum recorder_format format;
    AVFormatContext *ctx;
    struct size declared_frame_size;
    SDL_bool header_written;
};

SDL_bool recorder_init(struct recorder *recoder,
                       const char *filename,
                       enum recorder_format format,
                       struct size declared_frame_size);

void recorder_destroy(struct recorder *recorder);

SDL_bool recorder_open(struct recorder *recorder, AVCodec *input_codec);
void recorder_close(struct recorder *recorder);

SDL_bool recorder_write(struct recorder *recorder, AVPacket *packet);

#endif
