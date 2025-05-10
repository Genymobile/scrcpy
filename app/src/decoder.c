#include "decoder.h"

#include <errno.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>

#include "util/log.h"

/** Downcast packet_sink to decoder */
#define DOWNCAST(SINK) container_of(SINK, struct sc_decoder, packet_sink)

static bool
sc_decoder_open(struct sc_decoder *decoder, AVCodecContext *ctx,
                const struct sc_stream_session *session) {
    decoder->frame = av_frame_alloc();
    if (!decoder->frame) {
        LOG_OOM();
        return false;
    }

    if (!sc_frame_source_sinks_open(&decoder->frame_source, ctx, session)) {
        av_frame_free(&decoder->frame);
        return false;
    }

    decoder->ctx = ctx;

    return true;
}

static void
sc_decoder_close(struct sc_decoder *decoder) {
    sc_frame_source_sinks_close(&decoder->frame_source);
    av_frame_free(&decoder->frame);
}

static bool
sc_decoder_push(struct sc_decoder *decoder, const AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;
    if (is_config) {
        // nothing to do
        return true;
    }

    int ret = avcodec_send_packet(decoder->ctx, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        LOGE("Decoder '%s': could not send video packet: %d",
             decoder->name, ret);
        return false;
    }

    for (;;) {
        ret = avcodec_receive_frame(decoder->ctx, decoder->frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }

        if (ret) {
            LOGE("Decoder '%s', could not receive video frame: %d",
                 decoder->name, ret);
            return false;
        }

        // a frame was received
        bool ok = sc_frame_source_sinks_push(&decoder->frame_source,
                                             decoder->frame);
        av_frame_unref(decoder->frame);
        if (!ok) {
            // Error already logged
            return false;
        }
    }

    return true;
}

static bool
sc_decoder_push_session(struct sc_decoder *decoder,
                        const struct sc_stream_session *session) {
    return sc_frame_source_sinks_push_session(&decoder->frame_source, session);
}

static bool
sc_decoder_packet_sink_open(struct sc_packet_sink *sink, AVCodecContext *ctx,
                            const struct sc_stream_session *session) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    return sc_decoder_open(decoder, ctx, session);
}

static void
sc_decoder_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    sc_decoder_close(decoder);
}

static bool
sc_decoder_packet_sink_push(struct sc_packet_sink *sink,
                            const AVPacket *packet) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    return sc_decoder_push(decoder, packet);
}

static bool
sc_decoder_packet_sink_push_session(struct sc_packet_sink *sink,
                                    const struct sc_stream_session *session) {

    struct sc_decoder *decoder = DOWNCAST(sink);
    return sc_decoder_push_session(decoder, session);
}

void
sc_decoder_init(struct sc_decoder *decoder, const char *name) {
    decoder->name = name; // statically allocated
    sc_frame_source_init(&decoder->frame_source);

    static const struct sc_packet_sink_ops ops = {
        .open = sc_decoder_packet_sink_open,
        .close = sc_decoder_packet_sink_close,
        .push = sc_decoder_packet_sink_push,
        .push_session = sc_decoder_packet_sink_push_session,
    };

    decoder->packet_sink.ops = &ops;
}
