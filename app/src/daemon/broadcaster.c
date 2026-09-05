#include "broadcaster.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>

#include "daemon/codec.h"
#include "daemon/protocol.h"
#include "util/log.h"
#include "util/tick.h"

/** Downcast packet_sink to sc_broadcaster */
#define DOWNCAST(SINK) container_of(SINK, struct sc_broadcaster, packet_sink)

#define SC_BROADCASTER_KEEPALIVE_INTERVAL SC_TICK_FROM_SEC(5)
#define SC_BROADCASTER_MAX_GOP_BYTES (64 * 1024 * 1024)
// Building queue nodes is intentionally done under the broadcaster mutex so
// the GOP snapshot is atomic. Bound that work on the demuxer thread; extreme
// custom GOPs wait for the next keyframe instead of allocating tens of
// thousands of nodes synchronously.
#define SC_BROADCASTER_MAX_FAST_BOOTSTRAP_EVENTS 4096
#define SC_BROADCASTER_MAX_GOP_EVENTS \
    SC_BROADCASTER_MAX_FAST_BOOTSTRAP_EVENTS

static struct sc_broadcast_event *
event_new_meta(const struct sc_broadcaster *bc) {
    struct sc_broadcast_event *event = calloc(1, sizeof(*event));
    if (!event) {
        LOG_OOM();
        return NULL;
    }

    event->type = SC_BROADCAST_EVENT_META;
    snprintf(event->codec_name, sizeof(event->codec_name), "%s",
             bc->codec_name);
    event->width = bc->width;
    event->height = bc->height;
    event->client_resized = bc->client_resized;
    return event;
}

static struct sc_broadcast_packet *
packet_new(bool config, bool key, const uint8_t *data, size_t size) {
    if (size > SIZE_MAX - sizeof(struct sc_broadcast_packet)) {
        return NULL;
    }
    struct sc_broadcast_packet *packet = malloc(sizeof(*packet) + size);
    if (!packet) {
        LOG_OOM();
        return NULL;
    }

    atomic_init(&packet->refs, 1);
    packet->config = config;
    packet->key = key;
    packet->size = size;
    memcpy(packet->data, data, size);
    return packet;
}

static void
packet_ref(struct sc_broadcast_packet *packet) {
    atomic_fetch_add_explicit(&packet->refs, 1, memory_order_relaxed);
}

static void
packet_unref(struct sc_broadcast_packet *packet) {
    if (atomic_fetch_sub_explicit(&packet->refs, 1, memory_order_acq_rel)
            == 1) {
        free(packet);
    }
}

static struct sc_broadcast_event *
event_new_video(struct sc_broadcast_packet *packet) {
    struct sc_broadcast_event *event = malloc(sizeof(*event));
    if (!event) {
        LOG_OOM();
        return NULL;
    }

    *event = (struct sc_broadcast_event) {
        .next = NULL,
        .type = SC_BROADCAST_EVENT_VIDEO,
        .packet = packet,
    };
    packet_ref(packet);
    return event;
}

static void
event_delete(struct sc_broadcast_event *event) {
    if (event->packet) {
        packet_unref(event->packet);
    }
    free(event);
}

static size_t
event_logical_size(const struct sc_broadcast_event *event) {
    return sizeof(*event)
         + (event->packet ? event->packet->size : 0);
}

static void
queue_clear(struct sc_broadcast_subscriber *sub) {
    struct sc_broadcast_event *event = sub->head;
    while (event) {
        struct sc_broadcast_event *next = event->next;
        event_delete(event);
        event = next;
    }
    sub->head = NULL;
    sub->tail = NULL;
    sub->queued_bytes = 0;
    sub->queued_events = 0;
    sub->bootstrapping = false;
}

static bool
queue_push(struct sc_broadcast_subscriber *sub,
           struct sc_broadcast_event *event) {
    size_t event_bytes = event_logical_size(event);
    size_t max_bytes = sub->bootstrapping
                     ? SC_BROADCASTER_MAX_BOOTSTRAP_BYTES
                     : SC_BROADCASTER_MAX_QUEUE_BYTES;
    unsigned max_events = sub->bootstrapping
                        ? SC_BROADCASTER_MAX_BOOTSTRAP_EVENTS
                        : SC_BROADCASTER_MAX_QUEUE_EVENTS;
    // Permit one protocol-valid oversized keyframe/config even when it is
    // larger than the normal queue budget. The queue is still bounded by the
    // daemon's maximum single encoded-frame size.
    bool allowed_single_large_event =
        sub->queued_events <= 2
        && sub->queued_bytes <= 1024 * 1024
        && event->type == SC_BROADCAST_EVENT_VIDEO
        && (event->packet->key || event->packet->config)
        && event->packet->size <= SC_DAEMON_MAX_FRAME_SIZE;
    if (!allowed_single_large_event
            && (event_bytes > max_bytes
            || sub->queued_bytes
                    > max_bytes - event_bytes
            || sub->queued_events >= max_events)) {
        return false;
    }

    if (sub->tail) {
        sub->tail->next = event;
    } else {
        sub->head = event;
    }
    sub->tail = event;
    sub->queued_bytes += event_bytes;
    ++sub->queued_events;
    return true;
}

static struct sc_broadcast_event *
queue_pop(struct sc_broadcast_subscriber *sub) {
    struct sc_broadcast_event *event = sub->head;
    assert(event);
    sub->head = event->next;
    if (!sub->head) {
        sub->tail = NULL;
    }
    event->next = NULL;
    sub->queued_bytes -= event_logical_size(event);
    --sub->queued_events;
    if (sub->bootstrapping
            && sub->queued_bytes <= SC_BROADCASTER_MAX_QUEUE_BYTES
            && sub->queued_events <= SC_BROADCASTER_MAX_QUEUE_EVENTS) {
        sub->bootstrapping = false;
    }
    return event;
}

static struct sc_broadcast_subscriber *
find_subscriber(struct sc_broadcaster *bc, sc_socket socket) {
    for (unsigned i = 0; i < SC_BROADCASTER_MAX_SUBSCRIBERS; ++i) {
        struct sc_broadcast_subscriber *sub = &bc->subscribers[i];
        if (sub->active && sub->socket == socket) {
            return sub;
        }
    }
    return NULL;
}

static bool
enqueue_meta(struct sc_broadcaster *bc,
             struct sc_broadcast_subscriber *sub) {
    if (!bc->has_meta) {
        return true;
    }
    struct sc_broadcast_event *event = event_new_meta(bc);
    if (!event) {
        return false;
    }
    if (!queue_push(sub, event)) {
        event_delete(event);
        return false;
    }
    return true;
}

static bool
enqueue_video(struct sc_broadcast_subscriber *sub,
              struct sc_broadcast_packet *packet) {
    struct sc_broadcast_event *event = event_new_video(packet);
    if (!event) {
        return false;
    }
    if (!queue_push(sub, event)) {
        event_delete(event);
        return false;
    }
    return true;
}

// Queue a clean decoder bootstrap. Call with bc->mutex held.
static bool
enqueue_bootstrap(struct sc_broadcaster *bc,
                  struct sc_broadcast_subscriber *sub) {
    queue_clear(sub);
    sub->needs_key = true;
    sub->bootstrapping = bc->gop_head != NULL;

    if (!enqueue_meta(bc, sub)) {
        queue_clear(sub);
        return false;
    }
    if (!bc->gop_head) {
        return true;
    }
    if (bc->gop_events > SC_BROADCASTER_MAX_FAST_BOOTSTRAP_EVENTS) {
        // Keep needs_key=true. The next keyframe starts a small fresh GOP and
        // will bootstrap this subscriber without blocking the capture path.
        return true;
    }
    if (bc->config
            && !enqueue_video(sub, bc->config)) {
        queue_clear(sub);
        return false;
    }
    for (const struct sc_broadcast_event *event = bc->gop_head;
            event; event = event->next) {
        assert(event->type == SC_BROADCAST_EVENT_VIDEO);
        if (!enqueue_video(sub, event->packet)) {
            queue_clear(sub);
            return false;
        }
    }
    sub->needs_key = false;
    return true;
}

static void
clear_cached_gop(struct sc_broadcaster *bc) {
    struct sc_broadcast_event *event = bc->gop_head;
    while (event) {
        struct sc_broadcast_event *next = event->next;
        event_delete(event);
        event = next;
    }
    bc->gop_head = NULL;
    bc->gop_tail = NULL;
    bc->gop_bytes = 0;
    bc->gop_events = 0;
}

static void
reset_cached_stream(struct sc_broadcaster *bc) {
    if (bc->config) {
        packet_unref(bc->config);
    }
    bc->config = NULL;
    clear_cached_gop(bc);
}

static void
queue_new_session(struct sc_broadcaster *bc) {
    for (unsigned i = 0; i < SC_BROADCASTER_MAX_SUBSCRIBERS; ++i) {
        struct sc_broadcast_subscriber *sub = &bc->subscribers[i];
        if (!sub->active) {
            continue;
        }
        queue_clear(sub);
        sub->needs_key = true;
        // Sending metadata immediately tells an active decoder to discard the
        // old geometry. It will receive metadata again with the first fresh
        // keyframe; duplicate metadata is harmless and keeps recovery atomic.
        enqueue_meta(bc, sub);
    }
    sc_cond_broadcast(&bc->cond);
}

static bool
send_event(sc_socket socket, const struct sc_broadcast_event *event) {
    if (event->type == SC_BROADCAST_EVENT_META) {
        char json[160];
        int len = snprintf(json, sizeof(json),
                           "{\"event\":\"video_meta\",\"codec\":\"%s\","
                           "\"width\":%u,\"height\":%u,"
                           "\"client_resized\":%s}",
                           event->codec_name, event->width, event->height,
                           event->client_resized ? "true" : "false");
        return sc_daemon_write_frame(socket, json, len, NULL, 0);
    }

    const struct sc_broadcast_packet *packet = event->packet;
    char json[96];
    int len = snprintf(json, sizeof(json),
                       "{\"event\":\"video\",\"config\":%s,\"key\":%s,"
                       "\"payload_len\":%zu}",
                       packet->config ? "true" : "false",
                       packet->key ? "true" : "false", packet->size);
    return sc_daemon_write_frame(socket, json, len, packet->data,
                                 packet->size);
}

static bool
send_keepalive(sc_socket socket) {
    static const char json[] = "{\"event\":\"video_keepalive\"}";
    return sc_daemon_write_frame(socket, json, sizeof(json) - 1, NULL, 0);
}

static bool
sc_broadcaster_packet_sink_open(struct sc_packet_sink *sink,
                                AVCodecContext *ctx,
                                const struct sc_stream_session *session) {
    struct sc_broadcaster *bc = DOWNCAST(sink);

    const char *name = sc_daemon_avcodec_name(ctx->codec_id);
    if (!name) {
        LOGE("Broadcaster: unsupported video codec id %d", ctx->codec_id);
        return false;
    }

    sc_mutex_lock(&bc->mutex);
    bc->codec_name = name;
    bc->width = ctx->width;
    bc->height = ctx->height;
    bc->client_resized = session && session->video.client_resized;
    bc->has_meta = true;
    reset_cached_stream(bc);
    queue_new_session(bc);
    sc_mutex_unlock(&bc->mutex);

    return true;
}

static void
sc_broadcaster_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_broadcaster *bc = DOWNCAST(sink);

    sc_mutex_lock(&bc->mutex);
    bc->has_meta = false;
    reset_cached_stream(bc);
    for (unsigned i = 0; i < SC_BROADCASTER_MAX_SUBSCRIBERS; ++i) {
        struct sc_broadcast_subscriber *sub = &bc->subscribers[i];
        if (sub->active) {
            queue_clear(sub);
            sub->needs_key = true;
        }
    }
    // Keep subscribers connected: a new session will re-open and queue meta.
    sc_cond_broadcast(&bc->cond);
    sc_mutex_unlock(&bc->mutex);
}

static bool
sc_broadcaster_packet_sink_push_session(
        struct sc_packet_sink *sink,
        const struct sc_stream_session *session) {
    struct sc_broadcaster *bc = DOWNCAST(sink);

    sc_mutex_lock(&bc->mutex);
    bc->width = session->video.width;
    bc->height = session->video.height;
    bc->client_resized = session->video.client_resized;
    reset_cached_stream(bc);
    queue_new_session(bc);
    sc_mutex_unlock(&bc->mutex);
    return true;
}

static void
cache_config(struct sc_broadcaster *bc,
             struct sc_broadcast_packet *packet) {
    packet_ref(packet);
    if (bc->config) {
        packet_unref(bc->config);
    }
    bc->config = packet;
}

// Cache every packet from the most recent keyframe through the current frame.
// This lets a new subscriber replay a complete, reference-safe GOP before it
// joins the live tail. If the GOP itself exceeds the bounded cache, drop it
// and let new subscribers wait for the next keyframe.
static bool
cache_gop_packet(struct sc_broadcaster *bc,
                 struct sc_broadcast_packet *packet) {
    if (packet->key) {
        clear_cached_gop(bc);
    } else if (!bc->gop_head) {
        return false;
    }

    struct sc_broadcast_event *event = event_new_video(packet);
    if (!event) {
        clear_cached_gop(bc);
        return false;
    }

    size_t bytes = event_logical_size(event);
    bool allowed_single_large =
        !bc->gop_events && packet->size <= SC_DAEMON_MAX_FRAME_SIZE;
    if ((!allowed_single_large
            && (bytes > SC_BROADCASTER_MAX_GOP_BYTES
                || bc->gop_bytes
                        > SC_BROADCASTER_MAX_GOP_BYTES - bytes))
            || bc->gop_events >= SC_BROADCASTER_MAX_GOP_EVENTS) {
        event_delete(event);
        clear_cached_gop(bc);
        return false;
    }

    if (bc->gop_tail) {
        bc->gop_tail->next = event;
    } else {
        bc->gop_head = event;
    }
    bc->gop_tail = event;
    bc->gop_bytes += bytes;
    ++bc->gop_events;
    return true;
}

static bool
sc_broadcaster_packet_sink_push(struct sc_packet_sink *sink,
                                const AVPacket *packet) {
    struct sc_broadcaster *bc = DOWNCAST(sink);

    bool is_config = packet->pts == AV_NOPTS_VALUE;
    bool is_key = (packet->flags & AV_PKT_FLAG_KEY) != 0;
    struct sc_broadcast_packet *shared =
        packet_new(is_config, is_key, packet->data, packet->size);
    if (!shared) {
        // Never retain a GOP or subscriber queue across a missing encoded
        // packet: later deltas could reference it and make the bootstrap/live
        // stream undecodable. Broadcasting remains best-effort, but recovery
        // starts cleanly at the next keyframe.
        sc_mutex_lock(&bc->mutex);
        if (is_config) {
            reset_cached_stream(bc);
        } else {
            clear_cached_gop(bc);
        }
        for (unsigned i = 0; i < SC_BROADCASTER_MAX_SUBSCRIBERS; ++i) {
            struct sc_broadcast_subscriber *sub = &bc->subscribers[i];
            if (sub->active) {
                queue_clear(sub);
                sub->needs_key = true;
            }
        }
        sc_cond_broadcast(&bc->cond);
        sc_mutex_unlock(&bc->mutex);
        return true;
    }

    sc_mutex_lock(&bc->mutex);

    if (is_config) {
        // A new config invalidates the cached GOP generation.
        cache_config(bc, shared);
        clear_cached_gop(bc);
    } else {
        cache_gop_packet(bc, shared);
    }

    for (unsigned i = 0; i < SC_BROADCASTER_MAX_SUBSCRIBERS; ++i) {
        struct sc_broadcast_subscriber *sub = &bc->subscribers[i];
        if (!sub->active) {
            continue;
        }

        if (is_key && sub->needs_key) {
            if (!enqueue_bootstrap(bc, sub)) {
                sub->needs_key = true;
            }
            continue;
        }
        if (sub->needs_key) {
            // Do not feed deltas/config alone to a decoder after queue loss.
            continue;
        }

        if (!enqueue_video(sub, shared)) {
            // A slow subscriber is isolated: discard its pending deltas and
            // immediately rebuild it from the shared complete current GOP.
            // This avoids freezing for a full encoder I-frame interval.
            queue_clear(sub);
            sub->needs_key = true;
            if (bc->gop_head) {
                enqueue_bootstrap(bc, sub);
            }
        }
    }

    sc_cond_broadcast(&bc->cond);
    sc_mutex_unlock(&bc->mutex);
    packet_unref(shared);

    // Broadcasting is best-effort and never fails the device pipeline.
    return true;
}

bool
sc_broadcaster_init(struct sc_broadcaster *bc) {
    memset(bc, 0, sizeof(*bc));
    if (!sc_mutex_init(&bc->mutex)) {
        return false;
    }
    if (!sc_cond_init(&bc->cond)) {
        sc_mutex_destroy(&bc->mutex);
        return false;
    }

    static const struct sc_packet_sink_ops ops = {
        .open = sc_broadcaster_packet_sink_open,
        .close = sc_broadcaster_packet_sink_close,
        .push = sc_broadcaster_packet_sink_push,
        .push_session = sc_broadcaster_packet_sink_push_session,
    };
    bc->packet_sink.ops = &ops;
    return true;
}

void
sc_broadcaster_destroy(struct sc_broadcaster *bc) {
    sc_broadcaster_interrupt_all(bc);
    reset_cached_stream(bc);
    sc_cond_destroy(&bc->cond);
    sc_mutex_destroy(&bc->mutex);
}

bool
sc_broadcaster_subscribe(struct sc_broadcaster *bc, sc_socket socket) {
    sc_mutex_lock(&bc->mutex);

    struct sc_broadcast_subscriber *sub = NULL;
    for (unsigned i = 0; i < SC_BROADCASTER_MAX_SUBSCRIBERS; ++i) {
        if (!bc->subscribers[i].active) {
            sub = &bc->subscribers[i];
            break;
        }
    }
    if (!sub) {
        sc_mutex_unlock(&bc->mutex);
        LOGW("Broadcaster: too many video subscribers");
        return false;
    }

    *sub = (struct sc_broadcast_subscriber) {
        .active = true,
        .needs_key = true,
        .socket = socket,
    };
    if (!enqueue_bootstrap(bc, sub)) {
        queue_clear(sub);
        sub->active = false;
        sc_mutex_unlock(&bc->mutex);
        return false;
    }

    unsigned count = ++bc->sub_count;
    sc_cond_broadcast(&bc->cond);
    sc_mutex_unlock(&bc->mutex);

    LOGD("Broadcaster: new video subscriber (%u total)", count);
    return true;
}

bool
sc_broadcaster_run(struct sc_broadcaster *bc, sc_socket socket) {
    sc_tick keepalive_deadline =
        sc_tick_now() + SC_BROADCASTER_KEEPALIVE_INTERVAL;

    for (;;) {
        sc_mutex_lock(&bc->mutex);
        struct sc_broadcast_subscriber *sub =
            find_subscriber(bc, socket);
        if (!sub) {
            sc_mutex_unlock(&bc->mutex);
            return true;
        }

        while (sub->active && !sub->head) {
            bool signaled = sc_cond_timedwait(
                &bc->cond, &bc->mutex, keepalive_deadline);
            if (!sub->active) {
                sc_mutex_unlock(&bc->mutex);
                return true;
            }
            if ((!signaled || sc_tick_now() >= keepalive_deadline)
                    && !sub->head) {
                sc_mutex_unlock(&bc->mutex);
                if (!send_keepalive(socket)) {
                    sc_broadcaster_unsubscribe(bc, socket);
                    return false;
                }
                keepalive_deadline =
                    sc_tick_now() + SC_BROADCASTER_KEEPALIVE_INTERVAL;
                sc_mutex_lock(&bc->mutex);
                sub = find_subscriber(bc, socket);
                if (!sub) {
                    sc_mutex_unlock(&bc->mutex);
                    return true;
                }
            }
        }

        struct sc_broadcast_event *event = queue_pop(sub);
        sc_mutex_unlock(&bc->mutex);

        bool ok = send_event(socket, event);
        event_delete(event);
        if (!ok) {
            sc_broadcaster_unsubscribe(bc, socket);
            return false;
        }
        keepalive_deadline =
            sc_tick_now() + SC_BROADCASTER_KEEPALIVE_INTERVAL;
    }
}

void
sc_broadcaster_unsubscribe(struct sc_broadcaster *bc, sc_socket socket) {
    sc_mutex_lock(&bc->mutex);
    struct sc_broadcast_subscriber *sub = find_subscriber(bc, socket);
    if (sub) {
        queue_clear(sub);
        sub->active = false;
        sub->socket = SC_SOCKET_NONE;
        assert(bc->sub_count);
        --bc->sub_count;
        sc_cond_broadcast(&bc->cond);
    }
    unsigned count = bc->sub_count;
    sc_mutex_unlock(&bc->mutex);
    LOGD("Broadcaster: video subscriber removed (%u total)", count);
}

void
sc_broadcaster_interrupt_all(struct sc_broadcaster *bc) {
    sc_mutex_lock(&bc->mutex);
    for (unsigned i = 0; i < SC_BROADCASTER_MAX_SUBSCRIBERS; ++i) {
        struct sc_broadcast_subscriber *sub = &bc->subscribers[i];
        if (!sub->active) {
            continue;
        }
        net_interrupt(sub->socket);
        queue_clear(sub);
        sub->active = false;
        sub->socket = SC_SOCKET_NONE;
    }
    bc->sub_count = 0;
    sc_cond_broadcast(&bc->cond);
    sc_mutex_unlock(&bc->mutex);
}
