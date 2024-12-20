#ifndef SC_V4L2_SINK_H
#define SC_V4L2_SINK_H

#include "common.h"

#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "frame_buffer.h"
#include "trait/frame_sink.h"
#include "util/thread.h"

struct sc_v4l2_sink {
    struct sc_frame_sink frame_sink; // frame sink trait

    struct sc_frame_buffer fb;
    AVFormatContext *format_ctx;
    AVCodecContext *encoder_ctx;

    char *device_name;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    bool has_frame;
    bool stopped;
    bool header_written;

    AVFrame *frame;
    AVPacket *packet;
};

bool
sc_v4l2_sink_init(struct sc_v4l2_sink *vs, const char *device_name);

void
sc_v4l2_sink_destroy(struct sc_v4l2_sink *vs);

#endif
