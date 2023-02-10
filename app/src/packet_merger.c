#include "packet_merger.h"

#include "util/log.h"

void
sc_packet_merger_init(struct sc_packet_merger *merger) {
    merger->pending_config = NULL;
}

void
sc_packet_merger_destroy(struct sc_packet_merger *merger) {
    av_packet_free(&merger->pending_config);
}

bool
sc_packet_merger_merge(struct sc_packet_merger *merger, AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;

    if (is_config) {
        av_packet_free(&merger->pending_config);

        merger->pending_config = av_packet_alloc();
        if (!merger->pending_config) {
            LOG_OOM();
            goto error;
        }

        if (av_packet_ref(merger->pending_config, packet)) {
            LOG_OOM();
            av_packet_free(&merger->pending_config);
            goto error;
        }
    } else if (merger->pending_config) {
        size_t config_size = merger->pending_config->size;
        size_t media_size = packet->size;
        if (av_grow_packet(packet, config_size)) {
            LOG_OOM();
            goto error;
        }

        memmove(packet->data + config_size, packet->data, media_size);
        memcpy(packet->data, merger->pending_config->data, config_size);

        av_packet_free(&merger->pending_config);
    }

    return true;

error:
    av_packet_unref(packet);
    return false;
}
