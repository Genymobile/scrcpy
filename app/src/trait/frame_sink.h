#ifndef SC_FRAME_SINK_H
#define SC_FRAME_SINK_H

#include "common.h"

#include <stdbool.h>
#include <libavcodec/avcodec.h>

/**
 * Frame sink trait.
 *
 * Component able to receive AVFrames should implement this trait.
 */
struct sc_frame_sink {
    const struct sc_frame_sink_ops *ops;
};

struct sc_frame_sink_ops {
    /* The codec context is valid until the sink is closed */
    bool (*open)(struct sc_frame_sink *sink, const AVCodecContext *ctx);
    void (*close)(struct sc_frame_sink *sink);
    bool (*push)(struct sc_frame_sink *sink, const AVFrame *frame);
};

#endif
