#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdbool.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>

#include "config.h"
#include "common.h"
#include "util/queue.h"

struct capture_packet {
    AVPacket packet;
    struct capture_packet *next;
};

struct capture_queue QUEUE(struct capture_packet);

struct capture {
  char *filename;
  AVIOContext *file_context;
  AVCodecContext *context;
  AVFrame *working_frame;

  SDL_Thread *thread;
  SDL_mutex *mutex;
  SDL_cond *queue_cond;
  bool finished; // PNG has been captured
  bool stopped; // set on recorder_stop() by the stream reader
  struct capture_queue queue;
};

bool capture_init(struct capture *capture, const char *filename);

void capture_destroy(struct capture *capture);

bool
capture_start(struct capture *capture);

void
capture_stop(struct capture *capture);

void
capture_join(struct capture *capture);

bool capture_push(struct capture *capture, const AVPacket *packet);

#endif  // CAPTURE_H