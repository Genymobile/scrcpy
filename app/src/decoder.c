#include "decoder.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "events.h"
#include "video_buffer.h"
#include "trait/frame_sink.h"
#include "util/log.h"

/** Downcast packet_sink to decoder */
#define DOWNCAST(SINK) container_of(SINK, struct sc_decoder, packet_sink)

static void
sc_decoder_close_first_sinks(struct sc_decoder *decoder, unsigned count) {
    while (count) {
        struct sc_frame_sink *sink = decoder->sinks[--count];
        sink->ops->close(sink);
    }
}

static inline void
sc_decoder_close_sinks(struct sc_decoder *decoder) {
    sc_decoder_close_first_sinks(decoder, decoder->sink_count);
}

static bool
sc_decoder_open_sinks(struct sc_decoder *decoder) {
    for (unsigned i = 0; i < decoder->sink_count; ++i) {
        struct sc_frame_sink *sink = decoder->sinks[i];
        if (!sink->ops->open(sink)) {
            LOGE("Could not open frame sink %d", i);
            sc_decoder_close_first_sinks(decoder, i);
            return false;
        }
    }

    return true;
}

static bool
sc_decoder_open(struct sc_decoder *decoder, const AVCodec *codec) {
    decoder->codec_ctx = avcodec_alloc_context3(codec);
    if (!decoder->codec_ctx) {
        LOG_OOM();
        return false;
    }

    decoder->codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (avcodec_open2(decoder->codec_ctx, codec, NULL) < 0) {
        LOGE("Could not open codec");
        avcodec_free_context(&decoder->codec_ctx);
        return false;
    }

    decoder->frame = av_frame_alloc();
    if (!decoder->frame) {
        LOG_OOM();
        avcodec_close(decoder->codec_ctx);
        avcodec_free_context(&decoder->codec_ctx);
        return false;
    }

    if (!sc_decoder_open_sinks(decoder)) {
        LOGE("Could not open decoder sinks");
        av_frame_free(&decoder->frame);
        avcodec_close(decoder->codec_ctx);
        avcodec_free_context(&decoder->codec_ctx);
        return false;
    }

    return true;
}

static void
sc_decoder_close(struct sc_decoder *decoder) {
    sc_decoder_close_sinks(decoder);
    av_frame_free(&decoder->frame);
    avcodec_close(decoder->codec_ctx);
    avcodec_free_context(&decoder->codec_ctx);
}

static bool
push_frame_to_sinks(struct sc_decoder *decoder, const AVFrame *frame) {
    for (unsigned i = 0; i < decoder->sink_count; ++i) {
        struct sc_frame_sink *sink = decoder->sinks[i];
        if (!sink->ops->push(sink, frame)) {
            LOGE("Could not send frame to sink %d", i);
            return false;
        }
    }

    return true;
}

static bool
sc_decoder_push(struct sc_decoder *decoder, const AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;
    if (is_config) {
        // nothing to do
        return true;
    }

    int ret = avcodec_send_packet(decoder->codec_ctx, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        LOGE("Could not send video packet: %d", ret);
        return false;
    }
    ret = avcodec_receive_frame(decoder->codec_ctx, decoder->frame);
    if (!ret) {
        // a frame was received
        bool ok = push_frame_to_sinks(decoder, decoder->frame);
        // A frame lost should not make the whole pipeline fail. The error, if
        // any, is already logged.
        (void) ok;

        av_frame_unref(decoder->frame);
    } else if (ret != AVERROR(EAGAIN)) {
        LOGE("Could not receive video frame: %d", ret);
        return false;
    }
    return true;
}

static bool
sc_decoder_packet_sink_open(struct sc_packet_sink *sink, const AVCodec *codec) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    return sc_decoder_open(decoder, codec);
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

void
sc_decoder_init(struct sc_decoder *decoder) {
    decoder->sink_count = 0;

    static const struct sc_packet_sink_ops ops = {
        .open = sc_decoder_packet_sink_open,
        .close = sc_decoder_packet_sink_close,
        .push = sc_decoder_packet_sink_push,
    };

    decoder->packet_sink.ops = &ops;
}

void
sc_decoder_add_sink(struct sc_decoder *decoder, struct sc_frame_sink *sink) {
    assert(decoder->sink_count < SC_DECODER_MAX_SINKS);
    assert(sink);
    assert(sink->ops);
    decoder->sinks[decoder->sink_count++] = sink;
}
