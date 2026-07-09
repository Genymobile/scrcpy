#ifndef SC_DAEMON_BROADCASTER_H
#define SC_DAEMON_BROADCASTER_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trait/packet_sink.h"
#include "util/net.h"
#include "util/thread.h"

#define SC_BROADCASTER_MAX_SUBSCRIBERS 8

/**
 * Encoded-video broadcaster (doc/daemon.md, web streaming).
 *
 * A packet sink attached to the video demuxer that forwards the ENCODED
 * stream (not decoded frames) to subscribed IPC connections: codec metadata,
 * the config packet (SPS/PPS), then each encoded frame. This lets a browser
 * bridge decode with WebCodecs without the daemon decoding twice.
 *
 * Each forwarded item is a protocol frame (see protocol.h):
 *   {"event":"video_meta","codec":"h264","width":W,"height":H}
 *   {"event":"video","config":true,"payload_len":N} + N bytes (SPS/PPS)
 *   {"event":"video","key":BOOL,"payload_len":N}    + N bytes (frame)
 */
struct sc_broadcaster {
    struct sc_packet_sink packet_sink; // packet sink trait

    sc_mutex mutex; // guards everything below, held across subscriber writes

    // Codec metadata, set on open()
    const char *codec_name; // "h264" / "h265" / "av1" / NULL
    unsigned width;
    unsigned height;
    bool has_meta;

    // Latest config packet (SPS/PPS), forwarded to new subscribers
    uint8_t *config;
    size_t config_size;

    sc_socket subscribers[SC_BROADCASTER_MAX_SUBSCRIBERS];
    unsigned sub_count;
};

bool
sc_broadcaster_init(struct sc_broadcaster *bc);

void
sc_broadcaster_destroy(struct sc_broadcaster *bc);

/**
 * Register a subscriber: send it the current metadata and config packet (if
 * any), then start forwarding live frames. Returns false if the initial
 * writes fail or there is no room.
 */
bool
sc_broadcaster_subscribe(struct sc_broadcaster *bc, sc_socket socket);

/**
 * Unregister a subscriber (no-op if not registered).
 */
void
sc_broadcaster_unsubscribe(struct sc_broadcaster *bc, sc_socket socket);

#endif
