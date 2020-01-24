#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdbool.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>

struct capture {
  char *filename;
  AVIOContext *file_context;
  AVCodecContext *context;
  AVFrame *working_frame;
};

bool capture_init(struct capture *capture, const char *filename);

void capture_destroy(struct capture *capture);

bool capture_push(struct capture *capture, const AVPacket *packet);

#endif  // CAPTURE_H