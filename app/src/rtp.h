#ifndef SC_RTP_H
#define SC_RTP_H

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

#include "coords.h"
#include "trait/packet_sink.h"
#include "util/queue.h"
#include "util/thread.h"

struct sc_rtp_packet {
    AVPacket *packet;
    struct sc_rtp_packet *next;
};

struct sc_rtp_queue SC_QUEUE(struct sc_rtp_packet);

struct sc_rtp {
    struct sc_packet_sink packet_sink; // packet sink trait;

    char *out_url;
    AVFormatContext *ctx;
    struct sc_size declared_frame_size;
    bool header_written;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond queue_cond;
    bool stopped; // set on rtp_close()
    bool failed; // set on packet write failure
    struct sc_rtp_queue queue;
};

bool
sc_rtp_init(struct sc_rtp *rtp, const char *out_url,
            struct sc_size declared_frame_size);

void
sc_rtp_destroy(struct sc_rtp *rtp);

#endif
