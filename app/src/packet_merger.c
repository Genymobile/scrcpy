#include "packet_merger.h"

#include <stdlib.h>
#include <string.h>
#include <libavutil/avutil.h>

#include "util/log.h"

void
sc_packet_merger_init(struct sc_packet_merger *merger) {
    merger->config = NULL;
}

void
sc_packet_merger_destroy(struct sc_packet_merger *merger) {
    free(merger->config);
}

bool
sc_packet_merger_merge(struct sc_packet_merger *merger, AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;

    if (is_config) {
        free(merger->config);

        merger->config = malloc(packet->size);
        if (!merger->config) {
            LOG_OOM();
            return false;
        }

        memcpy(merger->config, packet->data, packet->size);
        merger->config_size = packet->size;
    } else if (merger->config) {
        size_t config_size = merger->config_size;
        size_t media_size = packet->size;

        if (av_grow_packet(packet, config_size)) {
            LOG_OOM();
            return false;
        }

        memmove(packet->data + config_size, packet->data, media_size);
        memcpy(packet->data, merger->config, config_size);

        free(merger->config);
        merger->config = NULL;
        // merger->size is meaningless when merger->config is NULL
    }

    return true;
}
