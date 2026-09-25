#ifndef SC_DECODER_H
#define SC_DECODER_H

#include "common.h"

#include <libavcodec/avcodec.h>

#include "coords.h"
#include "trait/frame_source.h"
#include "trait/packet_sink.h"
#include "util/thread.h"
#include "util/vecdeque.h"

enum sc_decoder_packet_type {
    SC_DECODER_PACKET_TYPE_AV_PACKET,
    SC_DECODER_PACKET_TYPE_SESSION,
};

struct sc_decoder_packet {
    enum sc_decoder_packet_type type;
    union {
        struct sc_stream_session session;
        AVPacket *packet;
    };
};

struct sc_decoder_queue SC_VECDEQUE(struct sc_decoder_packet);

struct sc_decoder {
    struct sc_packet_sink packet_sink; // packet sink trait
    struct sc_frame_source frame_source; // frame source trait

    const char *name; // must be statically allocated (e.g. a string literal)
    bool copy_opaque;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    // set on sc_decoder_stop(), packet_sink close or decoding failure
    bool stopped;
    // set by the decoder thread when it no longer uses the resources
    bool ended;
    struct sc_decoder_queue queue;

    AVCodecContext *ctx;
    AVFrame *frame;

    struct sc_stream_session session; // only initialized for video stream
    struct sc_size frame_size;

    const struct sc_decoder_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_decoder_callbacks {
    void (*on_ended)(struct sc_decoder *decoder, bool success, void *userdata);
};

// The name must be statically allocated (e.g. a string literal)
//
// copy_opaque: Set to true to forward the AVPacket.opaque_ref to the AVFrame
bool
sc_decoder_init(struct sc_decoder *decoder, const char *name, bool copy_opaque,
                const struct sc_decoder_callbacks *cbs, void *cbs_userdata);

bool
sc_decoder_start(struct sc_decoder *decoder);

void
sc_decoder_stop(struct sc_decoder *decoder);

void
sc_decoder_join(struct sc_decoder *decoder);

void
sc_decoder_destroy(struct sc_decoder *decoder);

#endif
