#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdbool.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include "trait/packet_sink.h"

#include "config.h"
#include "common.h"
#include "util/queue.h"
#include "util/thread.h"

struct capture_packet {
    AVPacket packet;
    struct capture_packet *next;
};

struct capture_queue QUEUE(struct capture_packet);

struct capture {
  struct sc_packet_sink packet_sink; // packet sink trait

  char *filename;
  AVIOContext *file_context;
  AVCodecContext *context;
  AVFrame *working_frame;

  sc_thread thread;
  sc_mutex mutex;
  sc_cond queue_cond;
  bool finished; // PNG has been captured
  bool stopped; // set on recorder_stop() by the stream reader
  struct capture_queue queue;
};

bool capture_init(struct capture *capture, const char *filename);

void capture_destroy(struct capture *capture);

#endif  // CAPTURE_H