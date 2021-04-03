#ifndef V4L2SINK_H
#define V4L2SINK_H

#include "config.h"

#ifdef V4L2SINK

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

#include "coords.h"
#include "scrcpy.h"
#include "util/queue.h"
#include "util/thread.h"

struct v4l2sink_packet {
    AVPacket packet;
    struct v4l2sink_packet *next;
};

struct v4l2sink_queue QUEUE(struct v4l2sink_packet);

struct v4l2sink {
    AVCodecContext *decoder_ctx;
    AVCodecContext *encoder_ctx;
    AVFrame *decoded_frame;
    AVPacket *raw_packet;

    char *devicename;
    char *filename;
    enum sc_record_format format;
    AVFormatContext *ctx;
    struct size declared_frame_size;
    bool header_written;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond queue_cond;
    bool stopped; // set on recorder_stop() by the stream reader
    bool failed; // set on packet write failure
    struct v4l2sink_queue queue;

    // we can write a packet only once we received the next one so that we can
    // set its duration (next_pts - current_pts)
    // "previous" is only accessed from the recorder thread, so it does not
    // need to be protected by the mutex
    struct v4l2sink_packet *previous;
};

bool
v4l2sink_init(struct v4l2sink *v4l2sink, const char *devicename, struct size declared_frame_size);

void
v4l2sink_destroy(struct v4l2sink *v4l2sink);

bool
v4l2sink_open(struct v4l2sink *v4l2sink, const AVCodec *input_codec);

void
v4l2sink_close(struct v4l2sink *v4l2sink);

bool
v4l2sink_start(struct v4l2sink *v4l2sink);

bool
v4l2sink_push(struct v4l2sink *v4l2sink, const AVPacket *packet);

void
v4l2sink_stop(struct v4l2sink *v4l2sink);

void
v4l2sink_join(struct v4l2sink *v4l2sink);

#else
struct v4l2sink { /* dummy struct for systems that don't support v4l2 */ };
#endif //V4L2SINK

#endif //V4L2SINK_H
