#ifndef SC_TCP_SINK_H
#define SC_TCP_SINK_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>

#include "trait/packet_sink.h"
#include "util/net.h"
#include "util/thread.h"
#include "util/vecdeque.h"

struct sc_tcp_sink_queue SC_VECDEQUE(AVPacket *);

struct sc_tcp_sink {
    struct sc_packet_sink packet_sink;
    uint16_t port;
    
    sc_socket server_socket;
    sc_socket client_socket;
    
    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    
    bool stopped;
    bool codec_sent;
    
    struct sc_tcp_sink_queue queue;
    
    // Codec information to send on connection
    uint32_t codec_id;
    uint32_t width;
    uint32_t height;
    
    // Cached config packet (SPS/PPS) to send to new clients
    AVPacket *config_packet;
};

bool
sc_tcp_sink_init(struct sc_tcp_sink *sink, uint16_t port);

bool
sc_tcp_sink_start(struct sc_tcp_sink *sink);

void
sc_tcp_sink_stop(struct sc_tcp_sink *sink);

void
sc_tcp_sink_join(struct sc_tcp_sink *sink);

void
sc_tcp_sink_destroy(struct sc_tcp_sink *sink);

#endif
