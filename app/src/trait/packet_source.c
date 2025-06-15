#include "packet_source.h"

#include <assert.h>

void
sc_packet_source_init(struct sc_packet_source *source) {
    source->sink_count = 0;
}

void
sc_packet_source_add_sink(struct sc_packet_source *source,
                          struct sc_packet_sink *sink) {
    assert(source->sink_count < SC_PACKET_SOURCE_MAX_SINKS);
    assert(sink);
    assert(sink->ops);
    source->sinks[source->sink_count++] = sink;
}

static void
sc_packet_source_sinks_close_firsts(struct sc_packet_source *source,
                                    unsigned count) {
    while (count) {
        struct sc_packet_sink *sink = source->sinks[--count];
        sink->ops->close(sink);
    }
}

bool
sc_packet_source_sinks_open(struct sc_packet_source *source,
                            AVCodecContext *ctx,
                            const struct sc_stream_session *session) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_packet_sink *sink = source->sinks[i];
        if (!sink->ops->open(sink, ctx, session)) {
            sc_packet_source_sinks_close_firsts(source, i);
            return false;
        }
    }

    return true;
}

void
sc_packet_source_sinks_close(struct sc_packet_source *source) {
    assert(source->sink_count);
    sc_packet_source_sinks_close_firsts(source, source->sink_count);
}

bool
sc_packet_source_sinks_push(struct sc_packet_source *source,
                            const AVPacket *packet) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_packet_sink *sink = source->sinks[i];
        if (!sink->ops->push(sink, packet)) {
            return false;
        }
    }

    return true;
}

bool
sc_packet_source_sinks_push_session(struct sc_packet_source *source,
                                    const struct sc_stream_session *session) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_packet_sink *sink = source->sinks[i];
        if (!sink->ops->push_session(sink, session)) {
            return false;
        }
    }

    return true;
}

void
sc_packet_source_sinks_disable(struct sc_packet_source *source) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_packet_sink *sink = source->sinks[i];
        if (sink->ops->disable) {
            sink->ops->disable(sink);
        }
    }
}
