#ifndef SC_DAEMON_BROADCASTER_H
#define SC_DAEMON_BROADCASTER_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "trait/packet_sink.h"
#include "util/net.h"
#include "util/thread.h"

#define SC_BROADCASTER_MAX_SUBSCRIBERS 8

/*
 * A slow browser must never back-pressure the device demuxer: otherwise it
 * would also freeze the decoder, screenshots, clips and report recording.
 * Each subscriber therefore owns a small bounded queue which is drained by
 * its connection thread. On overflow, delta frames are discarded until the
 * next keyframe.
 */
#define SC_BROADCASTER_MAX_QUEUE_BYTES (16 * 1024 * 1024)
#define SC_BROADCASTER_MAX_QUEUE_EVENTS 8192
#define SC_BROADCASTER_MAX_BOOTSTRAP_BYTES (80 * 1024 * 1024)
#define SC_BROADCASTER_MAX_BOOTSTRAP_EVENTS 4100

enum sc_broadcast_event_type {
    SC_BROADCAST_EVENT_META,
    SC_BROADCAST_EVENT_VIDEO,
};

struct sc_broadcast_packet {
    atomic_uint refs;
    bool config;
    bool key;
    size_t size;
    uint8_t data[];
};

struct sc_broadcast_event {
    struct sc_broadcast_event *next;
    enum sc_broadcast_event_type type;

    char codec_name[8];
    unsigned width;
    unsigned height;
    bool client_resized;

    // Immutable, reference-counted encoded payload. The GOP cache and every
    // subscriber queue share it instead of copying encoded bytes.
    struct sc_broadcast_packet *packet;
};

struct sc_broadcast_subscriber {
    bool active;
    bool needs_key;
    bool bootstrapping;
    sc_socket socket;

    struct sc_broadcast_event *head;
    struct sc_broadcast_event *tail;
    size_t queued_bytes;
    unsigned queued_events;
};

/**
 * Encoded-video broadcaster (doc/daemon.md, web streaming).
 *
 * A packet sink attached to the video demuxer that queues the ENCODED stream
 * (not decoded frames) for subscribed IPC connections: codec metadata, the
 * optional codec config packet, then encoded frames. The connection threads
 * perform all socket writes, so a slow or abandoned client cannot stall the
 * capture pipeline.
 *
 * The latest config and keyframe are cached. A new subscriber starts from
 * that decodable point without resetting the Android encoder, so opening or
 * refreshing a page does not introduce a recording/session boundary.
 */
struct sc_broadcaster {
    struct sc_packet_sink packet_sink; // packet sink trait

    sc_mutex mutex;
    sc_cond cond; // new queued event, subscriber removal or shutdown

    // Codec metadata, set on open()
    // "h264" / "h265" / "av1" / "vp8" / "vp9" / NULL
    const char *codec_name;
    unsigned width;
    unsigned height;
    bool client_resized;
    bool has_meta;

    // Latest config packet and complete current GOP, used to bootstrap new
    // subscribers. A keyframe alone is insufficient if it is followed by a
    // live delta which references omitted intermediate frames.
    struct sc_broadcast_packet *config;
    struct sc_broadcast_event *gop_head;
    struct sc_broadcast_event *gop_tail;
    size_t gop_bytes;
    unsigned gop_events;

    struct sc_broadcast_subscriber
        subscribers[SC_BROADCASTER_MAX_SUBSCRIBERS];
    unsigned sub_count;
};

bool
sc_broadcaster_init(struct sc_broadcaster *bc);

void
sc_broadcaster_destroy(struct sc_broadcaster *bc);

/**
 * Register a subscriber and queue the current metadata/config/keyframe.
 *
 * This function never writes to the socket. Returns false if there is no room
 * or the initial queue cannot be allocated.
 */
bool
sc_broadcaster_subscribe(struct sc_broadcaster *bc, sc_socket socket);

/**
 * Drain one subscriber queue to its socket until it disconnects or is
 * interrupted. Must be called by that subscriber's connection thread.
 */
bool
sc_broadcaster_run(struct sc_broadcaster *bc, sc_socket socket);

/**
 * Unregister a subscriber (no-op if not registered).
 */
void
sc_broadcaster_unsubscribe(struct sc_broadcaster *bc, sc_socket socket);

/**
 * Interrupt every subscriber and wake its connection thread.
 *
 * Used during daemon shutdown before joining client connections.
 */
void
sc_broadcaster_interrupt_all(struct sc_broadcaster *bc);

#endif
