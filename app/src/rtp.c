#include "rtp.h"

#include <assert.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

#include "util/log.h"

/** Downcast packet_sink to rtp */
#define DOWNCAST(SINK) container_of(SINK, struct sc_rtp, packet_sink)

static const AVRational SCRCPY_TIME_BASE = {1, 1000000}; // timestamps in us

static struct sc_rtp_packet *
sc_rtp_packet_new(const AVPacket *packet) {
    struct sc_rtp_packet *rtp = malloc(sizeof(*rtp));
    if (!rtp) {
        LOG_OOM();
        return NULL;
    }

    rtp->packet = av_packet_alloc();
    if (!rtp->packet) {
        LOG_OOM();
        free(rtp);
        return NULL;
    }

    if (av_packet_ref(rtp->packet, packet)) {
        av_packet_free(&rtp->packet);
        free(rtp);
        return NULL;
    }

    return rtp;
}

static void
sc_rtp_packet_delete(struct sc_rtp_packet *rtp) {
    av_packet_free(&rtp->packet);
    free(rtp);
}

static void
sc_rtp_queue_clear(struct sc_rtp_queue *queue) {
    while (!sc_queue_is_empty(queue)) {
        struct sc_rtp_packet *rtp;
        sc_queue_take(queue, next, &rtp);
        sc_rtp_packet_delete(rtp);
    }
}

static bool
sc_rtp_write_header(struct sc_rtp *rtp, const AVPacket *packet) {
    AVStream *ostream = rtp->ctx->streams[0];

    uint8_t *extradata = av_malloc(packet->size * sizeof(uint8_t));
    if (!extradata) {
        LOG_OOM();
        return false;
    }

    // copy the first packet to the extra data
    memcpy(extradata, packet->data, packet->size);

    ostream->codecpar->extradata = extradata;
    ostream->codecpar->extradata_size = packet->size;

    int ret = avformat_write_header(rtp->ctx, NULL);
    if (ret < 0) {
        LOGE("Failed to write RTP header");
        return false;
    }

    return true;
}

static void
sc_rtp_rescale_packet(struct sc_rtp *rtp, AVPacket *packet) {
    AVStream *ostream = rtp->ctx->streams[0];
    av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, ostream->time_base);
}

static bool
sc_rtp_write(struct sc_rtp *rtp, AVPacket *packet) {
    if (!rtp->header_written) {
        if (packet->pts != AV_NOPTS_VALUE) {
            LOGE("The first packet is not a config packet");
            return false;
        }
        bool ok = sc_rtp_write_header(rtp, packet);
        if (!ok) {
            return false;
        }
        rtp->header_written = true;
        return true;
    }

    if (packet->pts == AV_NOPTS_VALUE) {
        // ignore config packets
        return true;
    }

    sc_rtp_rescale_packet(rtp, packet);
    return av_write_frame(rtp->ctx, packet) >= 0;
}

static int
run_rtp(void *data) {
    struct sc_rtp *rtp = data;

    for (;;) {
        sc_mutex_lock(&rtp->mutex);

        while (!rtp->stopped && sc_queue_is_empty(&rtp->queue)) {
            sc_cond_wait(&rtp->queue_cond, &rtp->mutex);
        }

        // if stopped is set, continue to process the remaining events (to
        // finish the streaming) before actually stopping

        if (rtp->stopped && sc_queue_is_empty(&rtp->queue)) {
            sc_mutex_unlock(&rtp->mutex);
            break;
        }

        struct sc_rtp_packet *pkt;
        sc_queue_take(&rtp->queue, next, &pkt);

        sc_mutex_unlock(&rtp->mutex);

        bool ok = sc_rtp_write(rtp, pkt->packet);
        sc_rtp_packet_delete(pkt);
        if (!ok) {
            LOGE("Could not send packet");

            sc_mutex_lock(&rtp->mutex);
            rtp->failed = true;
            // discard pending packets
            sc_rtp_queue_clear(&rtp->queue);
            sc_mutex_unlock(&rtp->mutex);
            break;
        }
    }

    if (!rtp->failed) {
        if (rtp->header_written) {
            int ret = av_write_trailer(rtp->ctx);
            if (ret < 0) {
                LOGE("Failed to write RTP trailer");
                rtp->failed = true;
            }
        } else {
            // nothing has been sent
            rtp->failed = true;
        }
    }

    if (rtp->failed) {
        LOGE("Streaming over RTP failed");
    } else {
        LOGI("Streaming over RTP complete");
    }

    LOGD("RTP streaming thread ended");

    return 0;
}

static bool
sc_rtp_open(struct sc_rtp *rtp, const AVCodec *input_codec) {
    bool ok = sc_mutex_init(&rtp->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&rtp->queue_cond);
    if (!ok) {
        goto error_mutex_destroy;
    }

    sc_queue_init(&rtp->queue);
    rtp->stopped = false;
    rtp->failed = false;
    rtp->header_written = false;

    int ret = avformat_alloc_output_context2(&rtp->ctx, NULL, "rtp",
                                             rtp->out_url);
    if (ret < 0) {
        goto error_cond_destroy;
    }

    AVStream *ostream = avformat_new_stream(rtp->ctx, input_codec);
    if (!ostream) {
        goto error_avformat_free_context;
    }

    ostream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    ostream->codecpar->codec_id = input_codec->id;
    ostream->codecpar->width = rtp->declared_frame_size.width;
    ostream->codecpar->height = rtp->declared_frame_size.height;

    ret = avio_open(&rtp->ctx->pb, rtp->out_url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        LOGE("Failed to open output: %s", rtp->out_url);
        // ostream will be cleaned up during context cleaning
        goto error_avformat_free_context;
    }

    LOGD("Starting RTP thread");
    ok = sc_thread_create(&rtp->thread, run_rtp, "scrcpy-rtp", rtp);
    if (!ok) {
        LOGE("Could not start RTP thread");
        goto error_avio_close;
    }

    LOGI("Streaming started to %s", rtp->out_url);

    return true;

error_avio_close:
    avio_close(rtp->ctx->pb);
error_avformat_free_context:
    avformat_free_context(rtp->ctx);
error_cond_destroy:
    sc_cond_destroy(&rtp->queue_cond);
error_mutex_destroy:
    sc_mutex_destroy(&rtp->mutex);

    return false;
}

static void
sc_rtp_close(struct sc_rtp *rtp) {
    sc_mutex_lock(&rtp->mutex);
    rtp->stopped = true;
    sc_cond_signal(&rtp->queue_cond);
    sc_mutex_unlock(&rtp->mutex);

    sc_thread_join(&rtp->thread, NULL);

    avio_close(rtp->ctx->pb);
    avformat_free_context(rtp->ctx);
    sc_cond_destroy(&rtp->queue_cond);
    sc_mutex_destroy(&rtp->mutex);
}

static bool
sc_rtp_push(struct sc_rtp *rtp, const AVPacket *packet) {
    sc_mutex_lock(&rtp->mutex);
    assert(!rtp->stopped);

    if (rtp->failed) {
        // reject any new packet (this will stop the stream)
        sc_mutex_unlock(&rtp->mutex);
        return false;
    }

    struct sc_rtp_packet *pkt = sc_rtp_packet_new(packet);
    if (!pkt) {
        LOG_OOM();
        sc_mutex_unlock(&rtp->mutex);
        return false;
    }

    sc_queue_push(&rtp->queue, next, pkt);
    sc_cond_signal(&rtp->queue_cond);

    sc_mutex_unlock(&rtp->mutex);
    return true;
}

static bool
sc_rtp_packet_sink_open(struct sc_packet_sink *sink,
                        const AVCodec *codec) {
    struct sc_rtp *rtp = DOWNCAST(sink);
    return sc_rtp_open(rtp, codec);
}

static void
sc_rtp_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_rtp *rtp = DOWNCAST(sink);
    sc_rtp_close(rtp);
}

static bool
sc_rtp_packet_sink_push(struct sc_packet_sink *sink,
                        const AVPacket *packet) {
    struct sc_rtp *rtp = DOWNCAST(sink);
    return sc_rtp_push(rtp, packet);
}

bool
sc_rtp_init(struct sc_rtp *rtp, const char *out_url,
            struct sc_size declared_frame_size) {
    rtp->out_url = strdup(out_url);
    if (!rtp->out_url) {
        LOG_OOM();
        return false;
    }

    rtp->declared_frame_size = declared_frame_size;

    static const struct sc_packet_sink_ops ops = {
        .open = sc_rtp_packet_sink_open,
        .close = sc_rtp_packet_sink_close,
        .push = sc_rtp_packet_sink_push,
    };

    rtp->packet_sink.ops = &ops;

    return true;
}

void
sc_rtp_destroy(struct sc_rtp *rtp) {
    free(rtp->out_url);
}
