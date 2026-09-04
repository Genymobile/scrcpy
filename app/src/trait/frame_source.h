#ifndef SC_FRAME_SOURCE_H
#define SC_FRAME_SOURCE_H

#include "common.h"

#include <stdbool.h>

#include "trait/frame_sink.h"

#define SC_FRAME_SOURCE_MAX_SINKS 2

/**
 * Frame source trait
 *
 * Component able to send AVFrames should implement this trait.
 */
struct sc_frame_source {
    struct sc_frame_sink *sinks[SC_FRAME_SOURCE_MAX_SINKS];
    unsigned sink_count;
};

void
sc_frame_source_init(struct sc_frame_source *source);

void
sc_frame_source_add_sink(struct sc_frame_source *source,
                         struct sc_frame_sink *sink);

bool
sc_frame_source_sinks_open(struct sc_frame_source *source,
                           const AVCodecContext *ctx,
                           const struct sc_stream_session *session);

void
sc_frame_source_sinks_close(struct sc_frame_source *source);

bool
sc_frame_source_sinks_push(struct sc_frame_source *source,
                           const AVFrame *frame);

bool
sc_frame_source_sinks_push_session(struct sc_frame_source *source,
                                   const struct sc_stream_session *session);

#endif
