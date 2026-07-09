#ifndef SC_PACKET_SOURCE_H
#define SC_PACKET_SOURCE_H

#include "common.h"

#include <stdbool.h>

#include "trait/packet_sink.h"

// Fork: the daemon attaches up to 4 sinks to the video demuxer
// (decoder, web broadcaster, clip buffer, report recorder)
#define SC_PACKET_SOURCE_MAX_SINKS 4

/**
 * Packet source trait
 *
 * Component able to send AVPackets should implement this trait.
 */
struct sc_packet_source {
    struct sc_packet_sink *sinks[SC_PACKET_SOURCE_MAX_SINKS];
    unsigned sink_count;
};

void
sc_packet_source_init(struct sc_packet_source *source);

void
sc_packet_source_add_sink(struct sc_packet_source *source,
                          struct sc_packet_sink *sink);

bool
sc_packet_source_sinks_open(struct sc_packet_source *source,
                            AVCodecContext *ctx,
                            const struct sc_stream_session *session);

void
sc_packet_source_sinks_close(struct sc_packet_source *source);

bool
sc_packet_source_sinks_push(struct sc_packet_source *source,
                            const AVPacket *packet);

bool
sc_packet_source_sinks_push_session(struct sc_packet_source *source,
                                    const struct sc_stream_session *session);

void
sc_packet_source_sinks_disable(struct sc_packet_source *source);

#endif
