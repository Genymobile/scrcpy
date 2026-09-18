#include "decoder.h"

#include <errno.h>
#include <stdio.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>

#include "util/log.h"

/** Downcast packet_sink to decoder */
#define DOWNCAST(SINK) container_of(SINK, struct sc_decoder, packet_sink)

static bool
sc_decoder_open(struct sc_decoder *decoder, const AVCodec *codec,
                const AVCodecParameters *params,
                const struct sc_stream_session *session) {
    // A video stream must have a session
    assert(session || codec->type != AVMEDIA_TYPE_VIDEO);

    decoder->ctx = avcodec_alloc_context3(codec);
    if (!decoder->ctx) {
        LOG_OOM();
        return false;
    }

    int r = avcodec_parameters_to_context(decoder->ctx, params);
    if (r < 0) {
        LOGE("Decoder '%s': could not set codec parameters", decoder->name);
        goto error_free_context;
    }

    decoder->ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (decoder->copy_opaque) {
        // Propagate AVPacket.opaque_ref (the recv_date) to the decoded AVFrame
        decoder->ctx->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
    }

    r = avcodec_open2(decoder->ctx, codec, NULL);
    if (r < 0) {
        LOGE("Decoder '%s': could not open codec", decoder->name);
        goto error_free_context;
    }

    decoder->frame = av_frame_alloc();
    if (!decoder->frame) {
        LOG_OOM();
        goto error_free_context;
    }

    if (!sc_frame_source_sinks_open(&decoder->frame_source, decoder->ctx,
                                    session)) {
        goto error_free_frame;
    }

    if (session) {
        decoder->session = *session;
    }

    memset(&decoder->frame_size, 0, sizeof(decoder->frame_size));

    return true;

error_free_frame:
    av_frame_free(&decoder->frame);
error_free_context:
    avcodec_free_context(&decoder->ctx);

    return false;
}

static void
sc_decoder_close(struct sc_decoder *decoder) {
    sc_mutex_lock(&decoder->mutex);
    decoder->stopped = true;
    sc_cond_signal(&decoder->cond);

    // Wait until the decoder thread finished using the ctx, frame and the frame
    // sinks
    while (!decoder->ended) {
        sc_cond_wait(&decoder->cond, &decoder->mutex);
    }

    sc_mutex_unlock(&decoder->mutex);

    sc_frame_source_sinks_close(&decoder->frame_source);
    av_frame_free(&decoder->frame);
    avcodec_free_context(&decoder->ctx);
}

static enum sc_sink_result
sc_decoder_process_packet(struct sc_decoder *decoder, const AVPacket *packet) {
    // Do not decode more packets before the frame sinks are ready
    sc_frame_source_sinks_apply_backpressure(&decoder->frame_source);

    int ret = avcodec_send_packet(decoder->ctx, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        LOGE("Decoder '%s': could not send video packet: %s",
             decoder->name, av_err2str(ret));
        return SC_SINK_KO;
    }

    for (;;) {
        ret = avcodec_receive_frame(decoder->ctx, decoder->frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }

        if (ret) {
            LOGE("Decoder '%s', could not receive video frame: %s",
                 decoder->name, av_err2str(ret));
            return SC_SINK_KO;
        }

        // a frame was received

        if (decoder->ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            assert(decoder->frame->width >= 0);
            assert(decoder->frame->height >= 0);
            struct sc_size frame_size = {
                .width = decoder->frame->width,
                .height = decoder->frame->height,
            };
            if (decoder->frame_size.width != frame_size.width
                    || decoder->frame_size.height != frame_size.height) {
                // The frame size has changed, check if it matches the session
                uint32_t sw = decoder->session.video.width;
                uint32_t sh = decoder->session.video.height;
                if (frame_size.width != sw || frame_size.height != sh) {
                    LOGW("Unexpected video size: %" PRIu32 "x%" PRIu32
                         " (expected %" PRIu32 "x%" PRIu32 ")",
                         frame_size.width, frame_size.height, sw, sh);

                    LOGW("The encoder did not respect the requested size, "
                         "please retry with a lower resolution "
                         "(-m/--max-size)");
                }
            }

            decoder->frame_size = frame_size;
        }

        enum sc_sink_result result =
            sc_frame_source_sinks_push(&decoder->frame_source, decoder->frame);
        av_frame_unref(decoder->frame);
        if (result != SC_SINK_OK) {
            // Not a decoder error
            return SC_SINK_STOPPED;
        }
    }

    return SC_SINK_OK;
}

static void
sc_decoder_queue_clear(struct sc_decoder_queue *queue) {
    while (!sc_vecdeque_is_empty(queue)) {
        struct sc_decoder_packet *dp = sc_vecdeque_popref(queue);
        if (dp->type == SC_DECODER_PACKET_TYPE_AV_PACKET) {
            av_packet_free(&dp->packet);
        }
    }
}

static bool
sc_decoder_decode(struct sc_decoder *decoder) {
    for (;;) {
        sc_mutex_lock(&decoder->mutex);
        while (!decoder->stopped && sc_vecdeque_is_empty(&decoder->queue)) {
            sc_cond_wait(&decoder->cond, &decoder->mutex);
        }
        if (decoder->stopped) {
            sc_mutex_unlock(&decoder->mutex);
            return true;
        }

        struct sc_decoder_packet dp = sc_vecdeque_pop(&decoder->queue);
        sc_mutex_unlock(&decoder->mutex);

        enum sc_sink_result result;
        if (dp.type == SC_DECODER_PACKET_TYPE_AV_PACKET) {
            result = sc_decoder_process_packet(decoder, dp.packet);
            av_packet_free(&dp.packet);
        } else {
            assert(dp.type == SC_DECODER_PACKET_TYPE_SESSION);
            decoder->session = dp.session;
            enum sc_sink_result push_result =
                sc_frame_source_sinks_push_session(&decoder->frame_source,
                                                   &dp.session);
            // Not a decoder error
            result = push_result == SC_SINK_OK ? SC_SINK_OK : SC_SINK_STOPPED;
        }

        if (result == SC_SINK_STOPPED) {
            // No error
            return true;
        }

        if (result == SC_SINK_KO) {
            return false;
        }
    }
    return true;
}

static int
run_decoder(void *data) {
    struct sc_decoder *decoder = data;

    bool success = sc_decoder_decode(decoder);

    sc_mutex_lock(&decoder->mutex);
    // Prevent the producer from pushing any new packet
    decoder->stopped = true;
    // Discard pending packets
    sc_decoder_queue_clear(&decoder->queue);

    decoder->ended = true;
    sc_cond_signal(&decoder->cond);

    sc_mutex_unlock(&decoder->mutex);

    if (!success) {
        LOGE("Decoding (%s) failed", decoder->name);
    }

    LOGD("Decoder (%s) thread ended", decoder->name);

    decoder->cbs->on_ended(decoder, success, decoder->cbs_userdata);

    return 0;
}

static AVPacket *
sc_decoder_packet_ref(const AVPacket *packet) {
    AVPacket *p = av_packet_alloc();
    if (!p) {
        LOG_OOM();
        return NULL;
    }

    if (av_packet_ref(p, packet)) {
        LOG_OOM();
        av_packet_free(&p);
        return NULL;
    }

    return p;
}

static enum sc_sink_result
sc_decoder_push(struct sc_decoder *decoder, const AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;
    if (is_config) {
        // nothing to do
        return SC_SINK_OK;
    }

    sc_mutex_lock(&decoder->mutex);

    if (decoder->stopped) {
        // reject any new packet
        sc_mutex_unlock(&decoder->mutex);
        return SC_SINK_STOPPED;
    }

    AVPacket *p = sc_decoder_packet_ref(packet);
    if (!p) {
        sc_mutex_unlock(&decoder->mutex);
        return SC_SINK_KO;
    }

    struct sc_decoder_packet *dp =
        sc_vecdeque_push_uninitialized(&decoder->queue);
    if (!dp) {
        LOG_OOM();
        sc_mutex_unlock(&decoder->mutex);
        av_packet_free(&p);
        return SC_SINK_KO;
    }

    dp->type = SC_DECODER_PACKET_TYPE_AV_PACKET;
    dp->packet = p;

    sc_cond_signal(&decoder->cond);

    sc_mutex_unlock(&decoder->mutex);
    return SC_SINK_OK;
}

static enum sc_sink_result
sc_decoder_push_session(struct sc_decoder *decoder,
                        const struct sc_stream_session *session) {
    sc_mutex_lock(&decoder->mutex);

    if (decoder->stopped) {
        // reject any new packet
        sc_mutex_unlock(&decoder->mutex);
        return SC_SINK_STOPPED;
    }

    struct sc_decoder_packet *dp =
        sc_vecdeque_push_uninitialized(&decoder->queue);
    if (!dp) {
        LOG_OOM();
        sc_mutex_unlock(&decoder->mutex);
        return SC_SINK_KO;
    }

    dp->type = SC_DECODER_PACKET_TYPE_SESSION;
    dp->session = *session;

    sc_cond_signal(&decoder->cond);

    sc_mutex_unlock(&decoder->mutex);
    return SC_SINK_OK;
}

static bool
sc_decoder_packet_sink_open(struct sc_packet_sink *sink, const AVCodec *codec,
                            const AVCodecParameters *params,
                            const struct sc_stream_session *session) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    return sc_decoder_open(decoder, codec, params, session);
}

static void
sc_decoder_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    sc_decoder_close(decoder);
}

static enum sc_sink_result
sc_decoder_packet_sink_push(struct sc_packet_sink *sink,
                            const AVPacket *packet) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    return sc_decoder_push(decoder, packet);
}

static enum sc_sink_result
sc_decoder_packet_sink_push_session(struct sc_packet_sink *sink,
                                    const struct sc_stream_session *session) {
    struct sc_decoder *decoder = DOWNCAST(sink);
    return sc_decoder_push_session(decoder, session);
}

bool
sc_decoder_init(struct sc_decoder *decoder, const char *name, bool copy_opaque,
                const struct sc_decoder_callbacks *cbs, void *cbs_userdata) {
    bool ok = sc_mutex_init(&decoder->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&decoder->cond);
    if (!ok) {
        sc_mutex_destroy(&decoder->mutex);
        return false;
    }

    decoder->name = name; // statically allocated
    sc_frame_source_init(&decoder->frame_source);

    sc_vecdeque_init(&decoder->queue);
    decoder->stopped = false;
    decoder->ended = false;

    assert(cbs && cbs->on_ended);
    decoder->cbs = cbs;
    decoder->cbs_userdata = cbs_userdata;

    static const struct sc_packet_sink_ops ops = {
        .open = sc_decoder_packet_sink_open,
        .close = sc_decoder_packet_sink_close,
        .push = sc_decoder_packet_sink_push,
        .push_session = sc_decoder_packet_sink_push_session,
    };

    decoder->packet_sink.ops = &ops;
    decoder->copy_opaque = copy_opaque;

    return true;
}

bool
sc_decoder_start(struct sc_decoder *decoder) {
    char thread_name[16];
    // "scrcpy-vid-dec" or "scrcpy-aud-dec"
    snprintf(thread_name, sizeof(thread_name), "scrcpy-%.3s-dec",
             decoder->name);
    bool ok = sc_thread_create(&decoder->thread, run_decoder, thread_name,
                               decoder);
    if (!ok) {
        LOGE("Could not start decoder (%s) thread", decoder->name);
        return false;
    }

    return true;
}

void
sc_decoder_stop(struct sc_decoder *decoder) {
    sc_mutex_lock(&decoder->mutex);
    decoder->stopped = true;
    sc_cond_signal(&decoder->cond);
    sc_mutex_unlock(&decoder->mutex);
}

void
sc_decoder_join(struct sc_decoder *decoder) {
    sc_thread_join(&decoder->thread, NULL);
}

void
sc_decoder_destroy(struct sc_decoder *decoder) {
    sc_vecdeque_destroy(&decoder->queue);
    sc_cond_destroy(&decoder->cond);
    sc_mutex_destroy(&decoder->mutex);
}
