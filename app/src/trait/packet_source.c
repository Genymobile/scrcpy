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
                            const AVCodec *codec,
                            const AVCodecParameters *params,
                            const struct sc_stream_session *session) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_packet_sink *sink = source->sinks[i];
        if (!sink->ops->open(sink, codec, params, session)) {
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

enum sc_sink_result
sc_packet_source_sinks_push(struct sc_packet_source *source,
                            const AVPacket *packet) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_packet_sink *sink = source->sinks[i];
        enum sc_sink_result result = sink->ops->push(sink, packet);
        if (result != SC_SINK_OK) {
            return result;
        }
    }

    return SC_SINK_OK;
}

enum sc_sink_result
sc_packet_source_sinks_push_session(struct sc_packet_source *source,
                                    const struct sc_stream_session *session) {
    assert(source->sink_count);
    for (unsigned i = 0; i < source->sink_count; ++i) {
        struct sc_packet_sink *sink = source->sinks[i];
        if (sink->ops->push_session) {
            enum sc_sink_result result = sink->ops->push_session(sink, session);
            if (result != SC_SINK_OK) {
                return result;
            }
        }
    }

    return SC_SINK_OK;
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
