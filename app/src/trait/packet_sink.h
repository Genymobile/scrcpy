#ifndef SC_PACKET_SINK_H
#define SC_PACKET_SINK_H

#include "common.h"

#include <stdbool.h>
#include <libavcodec/avcodec.h>

/**
 * Packet sink trait.
 *
 * Component able to receive AVPackets should implement this trait.
 */
struct sc_packet_sink {
    const struct sc_packet_sink_ops *ops;
};

struct sc_stream_session_video {
    uint32_t width;
    uint32_t height;
};

struct sc_stream_session {
    struct sc_stream_session_video video;
};

struct sc_packet_sink_ops {
    /* The codec context is valid until the sink is closed */
    bool (*open)(struct sc_packet_sink *sink, AVCodecContext *ctx,
                 const struct sc_stream_session *session);
    void (*close)(struct sc_packet_sink *sink);
    bool (*push)(struct sc_packet_sink *sink, const AVPacket *packet);

    /**
     * Optional callback to be notified of a new stream session.
     */
    bool (*push_session)(struct sc_packet_sink *sink,
                         const struct sc_stream_session *session);

    /*/
     * Called when the input stream has been disabled at runtime.
     *
     * If it is called, then open(), close() and push() will never be called.
     *
     * It is useful to notify the recorder that the requested audio stream has
     * finally been disabled because the device could not capture it.
     */
    void (*disable)(struct sc_packet_sink *sink);
};

#endif
