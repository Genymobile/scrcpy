#ifndef SC_FRAME_SINK_H
#define SC_FRAME_SINK_H

#include "common.h"

#include <stdbool.h>
#include <libavcodec/avcodec.h>

#include "trait/packet_sink.h"
#include "trait/sink.h"

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
    bool
    (*open)(struct sc_frame_sink *sink, const AVCodecContext *ctx,
            const struct sc_stream_session *session);

    void
    (*close)(struct sc_frame_sink *sink);

    enum sc_sink_result
    (*push)(struct sc_frame_sink *sink, const AVFrame *frame);

    /**
     * Optional callback to be notified of a new stream session.
     */
    enum sc_sink_result
    (*push_session)(struct sc_frame_sink *sink,
                    const struct sc_stream_session *session);
};

#endif
