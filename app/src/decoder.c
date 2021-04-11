#include "decoder.h"

#include <libavformat/avformat.h>

#include "events.h"
#include "video_buffer.h"
#include "util/log.h"

/** Downcast packet_sink to decoder */
#define DOWNCAST(SINK) container_of(SINK, struct decoder, packet_sink)

bool
decoder_open(struct decoder *decoder, const AVCodec *codec) {
    decoder->codec_ctx = avcodec_alloc_context3(codec);
    if (!decoder->codec_ctx) {
        LOGC("Could not allocate decoder context");
        return false;
    }

    if (avcodec_open2(decoder->codec_ctx, codec, NULL) < 0) {
        LOGE("Could not open codec");
        avcodec_free_context(&decoder->codec_ctx);
        return false;
    }

    decoder->frame = av_frame_alloc();
    if (!decoder->frame) {
        LOGE("Could not create decoder frame");
        avcodec_close(decoder->codec_ctx);
        avcodec_free_context(&decoder->codec_ctx);
        return false;
    }

    return true;
}

void
decoder_close(struct decoder *decoder) {
    av_frame_free(&decoder->frame);
    avcodec_close(decoder->codec_ctx);
    avcodec_free_context(&decoder->codec_ctx);
}

static bool
decoder_push(struct decoder *decoder, const AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;
    if (is_config) {
        // nothing to do
        return true;
    }

    int ret;
    if ((ret = avcodec_send_packet(decoder->codec_ctx, packet)) < 0) {
        LOGE("Could not send video packet: %d", ret);
        return false;
    }
    ret = avcodec_receive_frame(decoder->codec_ctx, decoder->frame);
    if (!ret) {
        // a frame was received
        bool ok = video_buffer_push(decoder->video_buffer, decoder->frame);
        // A frame lost should not make the whole pipeline fail. The error, if
        // any, is already logged.
        (void) ok;
    } else if (ret != AVERROR(EAGAIN)) {
        LOGE("Could not receive video frame: %d", ret);
        return false;
    }
    return true;
}

static bool
decoder_packet_sink_open(struct sc_packet_sink *sink, const AVCodec *codec) {
    struct decoder *decoder = DOWNCAST(sink);
    return decoder_open(decoder, codec);
}

static void
decoder_packet_sink_close(struct sc_packet_sink *sink) {
    struct decoder *decoder = DOWNCAST(sink);
    decoder_close(decoder);
}

static bool
decoder_packet_sink_push(struct sc_packet_sink *sink, const AVPacket *packet) {
    struct decoder *decoder = DOWNCAST(sink);
    return decoder_push(decoder, packet);
}

void
decoder_init(struct decoder *decoder, struct video_buffer *vb) {
    decoder->video_buffer = vb;

    static const struct sc_packet_sink_ops ops = {
        .open = decoder_packet_sink_open,
        .close = decoder_packet_sink_close,
        .push = decoder_packet_sink_push,
    };

    decoder->packet_sink.ops = &ops;
}
