#ifndef SC_PACKET_MERGER_H
#define SC_PACKET_MERGER_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavcodec/packet.h>

/**
 * Config packets (containing the SPS/PPS) are sent in-band. A new config
 * packet is sent whenever a new encoding session is started (on start and on
 * device orientation change).
 *
 * Every time a config packet is received, it must be sent alone (for recorder
 * extradata), then concatenated to the next media packet (for correct decoding
 * and recording).
 *
 * This helper reads every input packet and modifies each media packet which
 * immediately follows a config packet to prepend the config packet payload.
 */

struct sc_packet_merger {
    uint8_t *config;
    size_t config_size;
};

void
sc_packet_merger_init(struct sc_packet_merger *merger);

void
sc_packet_merger_destroy(struct sc_packet_merger *merger);

/**
 * If the packet is a config packet, then keep its data for later.
 * Otherwise (if the packet is a media packet), then if a config packet is
 * pending, prepend the config packet to this packet (so the packet is
 * modified!).
 */
bool
sc_packet_merger_merge(struct sc_packet_merger *merger, AVPacket *packet);

#endif
