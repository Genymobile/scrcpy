#include "frame_source.h"

#include <assert.h>

void
sc_frame_source_init(struct sc_frame_source *source) {
    source->sink_count = 0;
}

void
sc_frame_source_add_sink(struct sc_frame_source *source,
                         struct sc_frame_sink *sink) {
    assert(source->sink_count < SC_FRAME_SOURCE_MAX_SINKS);
    assert(sink);
    assert(sink->ops);
    source->sinks[source->sink_count++] = sink;
}

static void
sc_frame_source_sinks_close_firsts(struct sc_frame_source *source,
                                    unsigned count) {
    while (count) {
        struct sc_frame_sink *sink = source->sinks[--count];
        sink->ops->close(sink);
    }
}

bool
sc_frame_source_sinks_open(struct sc_frame_source *source,
                           const AVCodecContext *ctx,
                           const struct sc_stream_session *session) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_frame_sink *sink = source->sinks[i];
        if (!sink->ops->open(sink, ctx, session)) {
            sc_frame_source_sinks_close_firsts(source, i);
            return false;
        }
    }

    return true;
}

void
sc_frame_source_sinks_close(struct sc_frame_source *source) {
    assert(source->sink_count);
    sc_frame_source_sinks_close_firsts(source, source->sink_count);
}

enum sc_sink_result
sc_frame_source_sinks_push(struct sc_frame_source *source,
                            const AVFrame *frame) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_frame_sink *sink = source->sinks[i];
        enum sc_sink_result result = sink->ops->push(sink, frame);
        if (result != SC_SINK_OK) {
            return result;
        }
    }

    return SC_SINK_OK;
}

enum sc_sink_result
sc_frame_source_sinks_push_session(struct sc_frame_source *source,
                                   const struct sc_stream_session *session) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_frame_sink *sink = source->sinks[i];
        if (sink->ops->push_session) {
            enum sc_sink_result result = sink->ops->push_session(sink, session);
            if (result != SC_SINK_OK) {
                return result;
            }
        }
    }

    return SC_SINK_OK;
}
