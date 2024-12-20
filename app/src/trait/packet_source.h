#ifndef SC_PACKET_SOURCE_H
#define SC_PACKET_SOURCE_H

#include "common.h"

#include <stdbool.h>

#include "trait/packet_sink.h"

#define SC_PACKET_SOURCE_MAX_SINKS 2

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
                            AVCodecContext *ctx);

void
sc_packet_source_sinks_close(struct sc_packet_source *source);

bool
sc_packet_source_sinks_push(struct sc_packet_source *source,
                            const AVPacket *packet);

void
sc_packet_source_sinks_disable(struct sc_packet_source *source);

#endif
