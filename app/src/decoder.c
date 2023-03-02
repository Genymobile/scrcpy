#include "decoder.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>

#include "events.h"
#include "trait/frame_sink.h"
#include "util/log.h"

/** Downcast packet_sink to decoder */
#define DOWNCAST(SINK) container_of(SINK, struct sc_decoder, packet_sink)

static bool
sc_decoder_open(struct sc_decoder *decoder, const AVCodec *codec) {
    decoder->codec_ctx = avcodec_alloc_context3(codec);
    if (!decoder->codec_ctx) {
        LOG_OOM();
        return false;
    }

    decoder->codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (codec->type == AVMEDIA_TYPE_VIDEO) {
        // Hardcoded video properties
        decoder->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    } else {
        // Hardcoded audio properties
        decoder->codec_ctx->ch_layout =
            (AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO;
        decoder->codec_ctx->sample_rate = 48000;
    }

    if (avcodec_open2(decoder->codec_ctx, codec, NULL) < 0) {
        LOGE("Decoder '%s': could not open codec", decoder->name);
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

    if (!sc_frame_source_sinks_open(&decoder->frame_source,
                                    decoder->codec_ctx)) {
        av_frame_free(&decoder->frame);
        avcodec_close(decoder->codec_ctx);
        avcodec_free_context(&decoder->codec_ctx);
        return false;
    }

    return true;
}

static void
sc_decoder_close(struct sc_decoder *decoder) {
    sc_frame_source_sinks_close(&decoder->frame_source);
    av_frame_free(&decoder->frame);
    avcodec_close(decoder->codec_ctx);
    avcodec_free_context(&decoder->codec_ctx);
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
        LOGE("Decoder '%s': could not send video packet: %d",
             decoder->name, ret);
        return false;
    }

    for (;;) {
        ret = avcodec_receive_frame(decoder->codec_ctx, decoder->frame);
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
        // A frame lost should not make the whole pipeline fail. The error, if
        // any, is already logged.
        (void) ok;

        av_frame_unref(decoder->frame);
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
sc_decoder_init(struct sc_decoder *decoder, const char *name) {
    decoder->name = name; // statically allocated
    sc_frame_source_init(&decoder->frame_source);

    static const struct sc_packet_sink_ops ops = {
        .open = sc_decoder_packet_sink_open,
        .close = sc_decoder_packet_sink_close,
        .push = sc_decoder_packet_sink_push,
    };

    decoder->packet_sink.ops = &ops;
}
