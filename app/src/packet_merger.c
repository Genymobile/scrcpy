#include "packet_merger.h"

#include <stdlib.h>
#include <string.h>
#include <libavutil/avutil.h>

#include "util/log.h"

void
sc_packet_merger_init(struct sc_packet_merger *merger) {
    merger->has_config = false;
    merger->config_size = 0;
}

void
sc_packet_merger_destroy(struct sc_packet_merger *merger) {
    (void) merger;
}

bool
sc_packet_merger_merge(struct sc_packet_merger *merger, AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;

    if (is_config) {
        if (packet->size > SC_PACKET_MERGER_CONFIG_MAX_SIZE) {
            LOGE("Config packet too large: %d bytes (max %d)",
                 packet->size, SC_PACKET_MERGER_CONFIG_MAX_SIZE);
            return false;
        }

        memcpy(merger->config, packet->data, packet->size);
        merger->config_size = packet->size;
        merger->has_config = true;
    } else if (merger->has_config) {
        size_t config_size = merger->config_size;
        size_t media_size = packet->size;

        if (av_grow_packet(packet, config_size)) {
            LOG_OOM();
            return false;
        }

        memmove(packet->data + config_size, packet->data, media_size);
        memcpy(packet->data, merger->config, config_size);

        merger->has_config = false;
    }

    return true;
}
