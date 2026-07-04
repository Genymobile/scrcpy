#include "broadcaster.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>

#include "daemon/protocol.h"
#include "util/log.h"

/** Downcast packet_sink to sc_broadcaster */
#define DOWNCAST(SINK) container_of(SINK, struct sc_broadcaster, packet_sink)

static const char *
codec_name(enum AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return "h264";
        case AV_CODEC_ID_HEVC: return "h265";
        case AV_CODEC_ID_AV1: return "av1";
        default: return NULL;
    }
}

// Must be called with bc->mutex held. Remove a subscriber by index and wake
// its reader so the connection thread tears it down.
static void
remove_subscriber(struct sc_broadcaster *bc, unsigned i) {
    assert(i < bc->sub_count);
    net_interrupt(bc->subscribers[i]);
    bc->subscribers[i] = bc->subscribers[bc->sub_count - 1];
    --bc->sub_count;
}

// Must be called with bc->mutex held. Send the metadata event to one socket.
static bool
send_meta(struct sc_broadcaster *bc, sc_socket socket) {
    char json[128];
    int len = snprintf(json, sizeof(json),
                       "{\"event\":\"video_meta\",\"codec\":\"%s\","
                       "\"width\":%u,\"height\":%u}",
                       bc->codec_name ? bc->codec_name : "h264",
                       bc->width, bc->height);
    return sc_daemon_write_frame(socket, json, len, NULL, 0);
}

// Must be called with bc->mutex held. Send one encoded packet to one socket.
static bool
send_packet(sc_socket socket, bool is_config, bool is_key,
            const uint8_t *data, size_t size) {
    char json[96];
    int len = snprintf(json, sizeof(json),
                       "{\"event\":\"video\",\"config\":%s,\"key\":%s,"
                       "\"payload_len\":%zu}",
                       is_config ? "true" : "false",
                       is_key ? "true" : "false", size);
    return sc_daemon_write_frame(socket, json, len, data, size);
}

static bool
sc_broadcaster_packet_sink_open(struct sc_packet_sink *sink,
                                AVCodecContext *ctx,
                                const struct sc_stream_session *session) {
    (void) session;
    struct sc_broadcaster *bc = DOWNCAST(sink);

    sc_mutex_lock(&bc->mutex);
    bc->codec_name = codec_name(ctx->codec_id);
    bc->width = ctx->width;
    bc->height = ctx->height;
    bc->has_meta = true;

    // New stream: drop the previous config, it will be re-sent as a packet
    free(bc->config);
    bc->config = NULL;
    bc->config_size = 0;

    // Notify existing subscribers of the (possibly new) geometry so they can
    // reconfigure their decoder
    for (unsigned i = 0; i < bc->sub_count;) {
        if (send_meta(bc, bc->subscribers[i])) {
            ++i;
        } else {
            remove_subscriber(bc, i);
        }
    }
    sc_mutex_unlock(&bc->mutex);

    return true;
}

static void
sc_broadcaster_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_broadcaster *bc = DOWNCAST(sink);

    sc_mutex_lock(&bc->mutex);
    bc->has_meta = false;
    // Keep subscribers connected: a new session will re-open and re-send meta
    sc_mutex_unlock(&bc->mutex);
}

static bool
sc_broadcaster_packet_sink_push(struct sc_packet_sink *sink,
                                const AVPacket *packet) {
    struct sc_broadcaster *bc = DOWNCAST(sink);

    bool is_config = packet->pts == AV_NOPTS_VALUE;
    bool is_key = (packet->flags & AV_PKT_FLAG_KEY) != 0;

    sc_mutex_lock(&bc->mutex);

    if (is_config) {
        // Store the config packet for future subscribers
        uint8_t *copy = malloc(packet->size);
        if (copy) {
            memcpy(copy, packet->data, packet->size);
            free(bc->config);
            bc->config = copy;
            bc->config_size = packet->size;
        } else {
            LOG_OOM();
        }
    }

    for (unsigned i = 0; i < bc->sub_count;) {
        if (send_packet(bc->subscribers[i], is_config, is_key, packet->data,
                        packet->size)) {
            ++i;
        } else {
            remove_subscriber(bc, i);
        }
    }

    sc_mutex_unlock(&bc->mutex);

    // Never fail the pipeline: broadcasting is best-effort
    return true;
}

bool
sc_broadcaster_init(struct sc_broadcaster *bc) {
    if (!sc_mutex_init(&bc->mutex)) {
        return false;
    }

    bc->codec_name = NULL;
    bc->width = 0;
    bc->height = 0;
    bc->has_meta = false;
    bc->config = NULL;
    bc->config_size = 0;
    bc->sub_count = 0;

    static const struct sc_packet_sink_ops ops = {
        .open = sc_broadcaster_packet_sink_open,
        .close = sc_broadcaster_packet_sink_close,
        .push = sc_broadcaster_packet_sink_push,
    };
    bc->packet_sink.ops = &ops;

    return true;
}

void
sc_broadcaster_destroy(struct sc_broadcaster *bc) {
    free(bc->config);
    sc_mutex_destroy(&bc->mutex);
}

bool
sc_broadcaster_subscribe(struct sc_broadcaster *bc, sc_socket socket) {
    sc_mutex_lock(&bc->mutex);

    if (bc->sub_count >= SC_BROADCASTER_MAX_SUBSCRIBERS) {
        sc_mutex_unlock(&bc->mutex);
        LOGW("Broadcaster: too many video subscribers");
        return false;
    }

    // Send current metadata and config so the client can init before the
    // next live frame. If has_meta is false (between sessions) the client
    // will receive video_meta once the next session opens.
    bool ok = true;
    if (bc->has_meta) {
        ok = send_meta(bc, socket);
        if (ok && bc->config) {
            ok = send_packet(socket, true, false, bc->config,
                             bc->config_size);
        }
    }

    if (!ok) {
        sc_mutex_unlock(&bc->mutex);
        return false;
    }

    bc->subscribers[bc->sub_count++] = socket;
    sc_mutex_unlock(&bc->mutex);

    LOGD("Broadcaster: new video subscriber (%u total)", bc->sub_count);
    return true;
}

void
sc_broadcaster_unsubscribe(struct sc_broadcaster *bc, sc_socket socket) {
    sc_mutex_lock(&bc->mutex);
    for (unsigned i = 0; i < bc->sub_count; ++i) {
        if (bc->subscribers[i] == socket) {
            // Do not interrupt: the caller owns the socket lifecycle here
            bc->subscribers[i] = bc->subscribers[bc->sub_count - 1];
            --bc->sub_count;
            break;
        }
    }
    sc_mutex_unlock(&bc->mutex);
}
