#include "tcp_sink.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util/binary.h"
#include "util/log.h"

#define DOWNCAST(SINK) container_of(SINK, struct sc_tcp_sink, packet_sink)

// Codec IDs matching scrcpy's demuxer protocol
#define SC_CODEC_ID_H264 UINT32_C(0x68323634) // "h264" in ASCII
#define SC_CODEC_ID_H265 UINT32_C(0x68323635) // "h265" in ASCII

#define SC_PACKET_FLAG_CONFIG    (UINT64_C(1) << 63)
#define SC_PACKET_FLAG_KEY_FRAME (UINT64_C(1) << 62)

static AVPacket *
sc_tcp_sink_packet_ref(const AVPacket *packet) {
    AVPacket *p = av_packet_alloc();
    if (!p) {
        LOG_OOM();
        return NULL;
    }

    if (av_packet_ref(p, packet)) {
        av_packet_free(&p);
        return NULL;
    }

    return p;
}

static void
sc_tcp_sink_queue_clear(struct sc_tcp_sink_queue *queue) {
    while (!sc_vecdeque_is_empty(queue)) {
        AVPacket *p = sc_vecdeque_pop(queue);
        av_packet_free(&p);
    }
}

static bool
sc_tcp_sink_send_codec_info(struct sc_tcp_sink *sink) {
    uint8_t buf[12];
    
    // Send codec ID (4 bytes)
    sc_write32be(buf, sink->codec_id);
    if (net_send_all(sink->client_socket, buf, 4) < 0) {
        return false;
    }
    
    // Send width and height (8 bytes)
    sc_write32be(buf, sink->width);
    sc_write32be(buf + 4, sink->height);
    if (net_send_all(sink->client_socket, buf, 8) < 0) {
        return false;
    }
    
    LOGI("TCP sink: sent codec info to client (codec=%08" PRIx32 ", %ux%u)",
         sink->codec_id, sink->width, sink->height);
    return true;
}

static bool
sc_tcp_sink_send_packet(struct sc_tcp_sink *sink, const AVPacket *packet) {
    uint8_t header[12];
    
    // Build PTS with flags
    uint64_t pts_flags;
    if (packet->pts == AV_NOPTS_VALUE) {
        // Config packet
        pts_flags = SC_PACKET_FLAG_CONFIG;
    } else {
        pts_flags = (uint64_t) packet->pts;
        if (packet->flags & AV_PKT_FLAG_KEY) {
            pts_flags |= SC_PACKET_FLAG_KEY_FRAME;
        }
    }
    
    // Write header
    sc_write64be(header, pts_flags);
    sc_write32be(header + 8, packet->size);
    
    // Send header
    if (net_send_all(sink->client_socket, header, 12) < 0) {
        return false;
    }
    
    // Send packet data
    if (net_send_all(sink->client_socket, packet->data, packet->size) < 0) {
        return false;
    }
    
    return true;
}

static int
run_tcp_sink(void *data) {
    struct sc_tcp_sink *sink = data;
    
    // Create server socket
    sink->server_socket = net_socket();
    if (sink->server_socket == SC_SOCKET_NONE) {
        LOGE("TCP sink: could not create server socket");
        return -1;
    }
    
    // Bind and listen
    if (!net_listen(sink->server_socket, IPV4_LOCALHOST, sink->port, 1)) {
        LOGE("TCP sink: could not listen on port %u", sink->port);
        net_close(sink->server_socket);
        return -1;
    }
    
    LOGI("TCP sink: listening on port %u", sink->port);
    
    while (!sink->stopped) {
        // Accept a client connection (blocking)
        sink->client_socket = net_accept(sink->server_socket);
        if (sink->client_socket == SC_SOCKET_NONE) {
            if (sink->stopped) {
                break;
            }
            LOGW("TCP sink: failed to accept client connection");
            continue;
        }
        
        LOGI("TCP sink: client connected");
        
        // Send codec info to the new client
        sc_mutex_lock(&sink->mutex);
        bool codec_info_available = sink->codec_sent;
        sc_mutex_unlock(&sink->mutex);
        
        if (codec_info_available) {
            if (!sc_tcp_sink_send_codec_info(sink)) {
                LOGW("TCP sink: failed to send codec info, client disconnected");
                net_close(sink->client_socket);
                sink->client_socket = SC_SOCKET_NONE;
                continue;
            }
            
            // Send cached config packet if available
            sc_mutex_lock(&sink->mutex);
            AVPacket *config_pkt = sink->config_packet;
            sc_mutex_unlock(&sink->mutex);
            
            if (config_pkt) {
                if (!sc_tcp_sink_send_packet(sink, config_pkt)) {
                    LOGW("TCP sink: failed to send config packet, client disconnected");
                    net_close(sink->client_socket);
                    sink->client_socket = SC_SOCKET_NONE;
                    continue;
                }
                LOGI("TCP sink: sent cached config packet to new client");
            }
        } else {
            // Codec info not yet available, wait for it
            sc_mutex_lock(&sink->mutex);
            while (!sink->codec_sent && !sink->stopped) {
                sc_cond_wait(&sink->cond, &sink->mutex);
            }
            sc_mutex_unlock(&sink->mutex);
            
            if (sink->stopped) {
                net_close(sink->client_socket);
                sink->client_socket = SC_SOCKET_NONE;
                break;
            }
            
            if (!sc_tcp_sink_send_codec_info(sink)) {
                LOGW("TCP sink: failed to send codec info, client disconnected");
                net_close(sink->client_socket);
                sink->client_socket = SC_SOCKET_NONE;
                continue;
            }
            
            // Send cached config packet if available
            AVPacket *config_pkt = sink->config_packet;
            if (config_pkt) {
                if (!sc_tcp_sink_send_packet(sink, config_pkt)) {
                    LOGW("TCP sink: failed to send config packet, client disconnected");
                    net_close(sink->client_socket);
                    sink->client_socket = SC_SOCKET_NONE;
                    continue;
                }
                LOGI("TCP sink: sent cached config packet to new client");
            }
        }
        
        // Process packets for this client
        bool client_connected = true;
        while (client_connected && !sink->stopped) {
            sc_mutex_lock(&sink->mutex);
            
            while (sc_vecdeque_is_empty(&sink->queue) && !sink->stopped) {
                sc_cond_wait(&sink->cond, &sink->mutex);
            }
            
            if (sink->stopped) {
                sc_mutex_unlock(&sink->mutex);
                break;
            }
            
            AVPacket *packet = sc_vecdeque_pop(&sink->queue);
            sc_mutex_unlock(&sink->mutex);
            
            if (!sc_tcp_sink_send_packet(sink, packet)) {
                LOGI("TCP sink: client disconnected");
                client_connected = false;
            }
            
            av_packet_free(&packet);
        }
        
        // Client disconnected or stopped
        if (sink->client_socket != SC_SOCKET_NONE) {
            net_close(sink->client_socket);
            sink->client_socket = SC_SOCKET_NONE;
        }
    }
    
    // Cleanup
    sc_mutex_lock(&sink->mutex);
    sc_tcp_sink_queue_clear(&sink->queue);
    sc_mutex_unlock(&sink->mutex);
    
    if (sink->server_socket != SC_SOCKET_NONE) {
        net_close(sink->server_socket);
        sink->server_socket = SC_SOCKET_NONE;
    }
    
    LOGD("TCP sink thread ended");
    return 0;
}

static bool
sc_tcp_sink_packet_sink_open(struct sc_packet_sink *sink_trait,
                              AVCodecContext *ctx) {
    struct sc_tcp_sink *sink = DOWNCAST(sink_trait);
    
    sc_mutex_lock(&sink->mutex);
    
    // Extract codec information
    switch (ctx->codec_id) {
        case AV_CODEC_ID_H264:
            sink->codec_id = SC_CODEC_ID_H264;
            break;
        case AV_CODEC_ID_HEVC:
            sink->codec_id = SC_CODEC_ID_H265;
            break;
        default:
            LOGE("TCP sink: unsupported codec");
            sc_mutex_unlock(&sink->mutex);
            return false;
    }
    
    sink->width = ctx->width;
    sink->height = ctx->height;
    sink->codec_sent = true;
    
    sc_cond_signal(&sink->cond);
    sc_mutex_unlock(&sink->mutex);
    
    LOGI("TCP sink: codec initialized");
    return true;
}

static void
sc_tcp_sink_packet_sink_close(struct sc_packet_sink *sink_trait) {
    struct sc_tcp_sink *sink = DOWNCAST(sink_trait);
    
    sc_mutex_lock(&sink->mutex);
    sink->stopped = true;
    sc_cond_signal(&sink->cond);
    sc_mutex_unlock(&sink->mutex);
}

static bool
sc_tcp_sink_packet_sink_push(struct sc_packet_sink *sink_trait,
                              const AVPacket *packet) {
    struct sc_tcp_sink *sink = DOWNCAST(sink_trait);
    
    sc_mutex_lock(&sink->mutex);
    
    if (sink->stopped) {
        sc_mutex_unlock(&sink->mutex);
        return false;
    }
    
    // Cache config packets for new clients
    if (packet->pts == AV_NOPTS_VALUE) {
        // This is a config packet - cache it
        if (sink->config_packet) {
            av_packet_free(&sink->config_packet);
        }
        sink->config_packet = sc_tcp_sink_packet_ref(packet);
        LOGI("TCP sink: cached config packet (size=%d)", packet->size);
    }
    
    // Only queue packets if a client is connected
    if (sink->client_socket == SC_SOCKET_NONE) {
        // No client connected, drop packet (but we cached config above)
        sc_mutex_unlock(&sink->mutex);
        return true;
    }
    
    AVPacket *pkt = sc_tcp_sink_packet_ref(packet);
    if (!pkt) {
        LOG_OOM();
        sc_mutex_unlock(&sink->mutex);
        return false;
    }
    
    bool ok = sc_vecdeque_push(&sink->queue, pkt);
    if (!ok) {
        LOG_OOM();
        av_packet_free(&pkt);
        sc_mutex_unlock(&sink->mutex);
        return false;
    }
    
    sc_cond_signal(&sink->cond);
    sc_mutex_unlock(&sink->mutex);
    
    return true;
}

bool
sc_tcp_sink_init(struct sc_tcp_sink *sink, uint16_t port) {
    sink->port = port;
    sink->server_socket = SC_SOCKET_NONE;
    sink->client_socket = SC_SOCKET_NONE;
    sink->stopped = false;
    sink->codec_sent = false;
    sink->config_packet = NULL;
    
    bool ok = sc_mutex_init(&sink->mutex);
    if (!ok) {
        return false;
    }
    
    ok = sc_cond_init(&sink->cond);
    if (!ok) {
        sc_mutex_destroy(&sink->mutex);
        return false;
    }
    
    sc_vecdeque_init(&sink->queue);
    
    static const struct sc_packet_sink_ops ops = {
        .open = sc_tcp_sink_packet_sink_open,
        .close = sc_tcp_sink_packet_sink_close,
        .push = sc_tcp_sink_packet_sink_push,
    };
    
    sink->packet_sink.ops = &ops;
    
    return true;
}

bool
sc_tcp_sink_start(struct sc_tcp_sink *sink) {
    bool ok = sc_thread_create(&sink->thread, run_tcp_sink, "tcp-sink", sink);
    if (!ok) {
        LOGE("Could not start TCP sink thread");
        return false;
    }
    
    return true;
}

void
sc_tcp_sink_stop(struct sc_tcp_sink *sink) {
    sc_mutex_lock(&sink->mutex);
    sink->stopped = true;
    sc_cond_signal(&sink->cond);
    sc_mutex_unlock(&sink->mutex);
    
    // Interrupt server socket to unblock accept()
    if (sink->server_socket != SC_SOCKET_NONE) {
        net_interrupt(sink->server_socket);
    }
    
    // Interrupt client socket to unblock send()
    if (sink->client_socket != SC_SOCKET_NONE) {
        net_interrupt(sink->client_socket);
    }
}

void
sc_tcp_sink_join(struct sc_tcp_sink *sink) {
    sc_thread_join(&sink->thread, NULL);
}

void
sc_tcp_sink_destroy(struct sc_tcp_sink *sink) {
    sc_tcp_sink_queue_clear(&sink->queue);
    
    // Free cached config packet
    if (sink->config_packet) {
        av_packet_free(&sink->config_packet);
        sink->config_packet = NULL;
    }
    
    sc_cond_destroy(&sink->cond);
    sc_mutex_destroy(&sink->mutex);
}
