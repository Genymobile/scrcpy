#include "config.h"

#ifdef V4L2SINK
#include "v4l2sink.h"

#include <assert.h>
#include <libavutil/time.h>

#include "util/log.h"

static const AVRational SCRCPY_TIME_BASE = {1, 1000000}; // timestamps in us

static const AVOutputFormat *
find_muxer(const char *name) {
#ifdef SCRCPY_LAVF_HAS_NEW_MUXER_ITERATOR_API
    void *opaque = NULL;
#endif
    const AVOutputFormat *oformat = NULL;
    do {
#ifdef SCRCPY_LAVF_HAS_NEW_MUXER_ITERATOR_API
        oformat = av_muxer_iterate(&opaque);
#else
        oformat = av_oformat_next(oformat);
#endif
        // until null or with name "v4l2"
    } while (oformat && !strstr(oformat->name, name));
    return oformat;
}

static struct v4l2sink_packet *
v4l2sink_packet_new(const AVPacket *packet) {
    struct v4l2sink_packet *rec = malloc(sizeof(*rec));
    if (!rec) {
        return NULL;
    }

    // av_packet_ref() does not initialize all fields in old FFmpeg versions
    // See <https://github.com/Genymobile/scrcpy/issues/707>
    av_init_packet(&rec->packet);

    if (av_packet_ref(&rec->packet, packet)) {
        free(rec);
        return NULL;
    }
    return rec;
}

static void
v4l2sink_packet_delete(struct v4l2sink_packet *rec) {
    av_packet_unref(&rec->packet);
    free(rec);
}

static void
v4l2sink_queue_clear(struct v4l2sink_queue *queue) {
    while (!queue_is_empty(queue)) {
        struct v4l2sink_packet *rec;
        queue_take(queue, next, &rec);
        v4l2sink_packet_delete(rec);
    }
}

bool
v4l2sink_init(struct v4l2sink *v4l2sink,
              const char *devicename,
              struct size declared_frame_size) {
    v4l2sink->devicename = strdup(devicename);
    if (!v4l2sink->devicename) {
        LOGE("Could not strdup devicename for v4l2sink");
        return false;
    }

    bool ok = sc_mutex_init(&v4l2sink->mutex);
    if (!ok) {
        LOGC("Could not create mutex for v4l2sink");
        free(v4l2sink->devicename);
        return false;
    }

    ok = sc_cond_init(&v4l2sink->queue_cond);
    if (!ok) {
        LOGC("Could not create cond for v4l2sink");
        sc_mutex_destroy(&v4l2sink->mutex);
        free(v4l2sink->devicename);
        return false;
    }

    queue_init(&v4l2sink->queue);
    v4l2sink->stopped = false;
    v4l2sink->failed = false;
    v4l2sink->declared_frame_size = declared_frame_size;
    v4l2sink->header_written = false;
    v4l2sink->previous = NULL;

    return true;
}

void
v4l2sink_destroy(struct v4l2sink *v4l2sink) {
    sc_cond_destroy(&v4l2sink->queue_cond);
    sc_mutex_destroy(&v4l2sink->mutex);
    free(v4l2sink->devicename);
}

bool
v4l2sink_open(struct v4l2sink *v4l2sink, const AVCodec *codec) {
    v4l2sink->decoder_ctx = avcodec_alloc_context3(codec);
    if (!v4l2sink->decoder_ctx) {
        LOGC("Could not allocate decoder context for v4l2sink");
        return false;
    }

    if (avcodec_open2(v4l2sink->decoder_ctx, codec, NULL) < 0) {
        LOGE("Could not open decoder codec for v4l2sink");
        avcodec_free_context(&v4l2sink->decoder_ctx);
        return false;
    }

    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_RAWVIDEO);
    if (!encoder) {
        LOGE("RAWVIDEO encoder not found");
        return false;
    }

    v4l2sink->encoder_ctx = avcodec_alloc_context3(encoder);
    if (!v4l2sink->encoder_ctx) {
        LOGC("Could not allocate encoder context for v4l2sink");
        return false;
    }

    v4l2sink->encoder_ctx->width=v4l2sink->declared_frame_size.width;
    v4l2sink->encoder_ctx->height=v4l2sink->declared_frame_size.height;
    v4l2sink->encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    v4l2sink->encoder_ctx->time_base.num =1;

    if (avcodec_open2(v4l2sink->encoder_ctx, encoder, NULL) < 0) {
        LOGE("Could not open encoder codec for v4l2sink");
        avcodec_free_context(&v4l2sink->encoder_ctx);
        return false;
    }

    const AVOutputFormat *format = find_muxer("v4l2");
    if (!format) {
        LOGE("Could not find muxer for v4l2sink");
        return false;
    }

    v4l2sink->ctx = avformat_alloc_context();
    if (!v4l2sink->ctx) {
        LOGE("Could not allocate output context for v4l2sink");
        return false;
    }

    // contrary to the deprecated API (av_oformat_next()), av_muxer_iterate()
    // returns (on purpose) a pointer-to-const, but AVFormatContext.oformat
    // still expects a pointer-to-non-const (it has not be updated accordingly)
    // <https://github.com/FFmpeg/FFmpeg/commit/0694d8702421e7aff1340038559c438b61bb30dd>
    v4l2sink->ctx->oformat = (AVOutputFormat *) format;

    av_dict_set(&v4l2sink->ctx->metadata, "comment",
                "Recorded by scrcpy " SCRCPY_VERSION, 0);

    AVStream *ostream = avformat_new_stream(v4l2sink->ctx, encoder);
    if (!ostream) {
        avformat_free_context(v4l2sink->ctx);
        return false;
    }

#ifdef SCRCPY_LAVF_HAS_NEW_CODEC_PARAMS_API
    ostream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    ostream->codecpar->codec_id = encoder->id;
    ostream->codecpar->format = AV_PIX_FMT_YUV420P;
    ostream->codecpar->width = v4l2sink->declared_frame_size.width;
    ostream->codecpar->height = v4l2sink->declared_frame_size.height;
#else
    ostream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    ostream->codec->codec_id = encoder->id;
    ostream->codec->pix_fmt = AV_PIX_FMT_YUV420P;
    ostream->codec->width = v4l2sink->declared_frame_size.width;
    ostream->codec->height = v4l2sink->declared_frame_size.height;
#endif

    int ret = avio_open(&v4l2sink->ctx->pb, v4l2sink->devicename,
                        AVIO_FLAG_WRITE);
    if (ret < 0) {
        LOGE("Failed to open output device: %s", v4l2sink->devicename);
        // ostream will be cleaned up during context cleaning
        avformat_free_context(v4l2sink->ctx);
        return false;
    }

    v4l2sink->decoded_frame = av_frame_alloc();
    v4l2sink->raw_packet = av_packet_alloc();

    LOGI("V4l2sink started to device: %s", v4l2sink->devicename);

    return true;
}

void
v4l2sink_close(struct v4l2sink *v4l2sink) {
    avcodec_close(v4l2sink->decoder_ctx);
    avcodec_free_context(&v4l2sink->decoder_ctx);

    avcodec_close(v4l2sink->encoder_ctx);
    avcodec_free_context(&v4l2sink->encoder_ctx);

    if (v4l2sink->header_written) {
        int ret = av_write_trailer(v4l2sink->ctx);
        if (ret < 0) {
            LOGE("Failed to write trailer to %s", v4l2sink->devicename);
            v4l2sink->failed = true;
        }
    } else {
        v4l2sink->failed = true;
    }
    avio_close(v4l2sink->ctx->pb);
    avformat_free_context(v4l2sink->ctx);

    if (v4l2sink->failed) {
        LOGE("Sink failed to %s", v4l2sink->devicename);
    } else {
        LOGI("Sink completed device: %s", v4l2sink->devicename);
    }

    if (v4l2sink->decoded_frame) {
        av_frame_free(&v4l2sink->decoded_frame);
    }

    if (v4l2sink->raw_packet) {
        av_packet_free(&v4l2sink->raw_packet);
    }
}

static bool
v4l2sink_write_header(struct v4l2sink *v4l2sink, const AVPacket *packet) {
    AVStream *ostream = v4l2sink->ctx->streams[0];

    uint8_t *extradata = av_malloc(packet->size * sizeof(uint8_t));
    if (!extradata) {
        LOGC("Could not allocate extradata");
        return false;
    }

    // copy the first packet to the extra data
    memcpy(extradata, packet->data, packet->size);

#ifdef SCRCPY_LAVF_HAS_NEW_CODEC_PARAMS_API
    ostream->codecpar->extradata = extradata;
    ostream->codecpar->extradata_size = packet->size;
#else
    ostream->codec->extradata = extradata;
    ostream->codec->extradata_size = packet->size;
#endif

    v4l2sink->ctx->url = strdup(v4l2sink->devicename);

    int ret = avformat_write_header(v4l2sink->ctx, NULL);
    if (ret < 0) {
        LOGE("Failed to write header to %s", v4l2sink->devicename);
        return false;
    }

    return true;
}

static void
v4l2sink_rescale_packet(struct v4l2sink *v4l2sink, AVPacket *packet) {
    AVStream *ostream = v4l2sink->ctx->streams[0];
    av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, ostream->time_base);
}

bool
v4l2sink_write(struct v4l2sink *v4l2sink, AVPacket *packet) {
    if (!v4l2sink->header_written) {
        bool ok = v4l2sink_write_header(v4l2sink, packet);
        if (!ok) {
            return false;
        }
        v4l2sink->header_written = true;
    }

    if (packet->pts == AV_NOPTS_VALUE) {
        // ignore config packets
        return true;
    }

    v4l2sink_rescale_packet(v4l2sink, packet);
    return av_write_frame(v4l2sink->ctx, packet) >= 0;
}

static int
run_v4l2sink(void *data) {
    struct v4l2sink *v4l2sink = data;

    for (;;) {
        sc_mutex_lock(&v4l2sink->mutex);

        while (!v4l2sink->stopped && queue_is_empty(&v4l2sink->queue)) {
            sc_cond_wait(&v4l2sink->queue_cond, &v4l2sink->mutex);
        }

        // if stopped is set, continue to process the remaining events before actually stopping

        if (v4l2sink->stopped && queue_is_empty(&v4l2sink->queue)) {
            sc_mutex_unlock(&v4l2sink->mutex);
            struct v4l2sink_packet *last = v4l2sink->previous;
            if (last) {
                // assign an arbitrary duration to the last packet
                last->packet.duration = 100000;
                bool ok = v4l2sink_write(v4l2sink, &last->packet);
                if (!ok) {
                    // failing to write the last frame is not very serious, no
                    // future frame may depend on it, so the resulting file
                    // will still be valid
                    LOGW("Could not send last packet to v4l2sink");
                }
                v4l2sink_packet_delete(last);
            }
            break;
        }

        struct v4l2sink_packet *rec;
        queue_take(&v4l2sink->queue, next, &rec);

        sc_mutex_unlock(&v4l2sink->mutex);

        // v4l2sink->previous is only written from this thread, no need to lock
        struct v4l2sink_packet *previous = v4l2sink->previous;
        v4l2sink->previous = rec;

        if (!previous) {
            // we just received the first packet
            continue;
        }

        // config packets have no PTS, we must ignore them
        if (rec->packet.pts != AV_NOPTS_VALUE
            && previous->packet.pts != AV_NOPTS_VALUE) {
            // we now know the duration of the previous packet
            previous->packet.duration = rec->packet.pts - previous->packet.pts;
        }

        bool ok = v4l2sink_write(v4l2sink, &previous->packet);
        v4l2sink_packet_delete(previous);
        if (!ok) {
            LOGE("V4l2sink: Could not decode packet");

            sc_mutex_lock(&v4l2sink->mutex);
            v4l2sink->failed = true;
            // discard pending packets
            v4l2sink_queue_clear(&v4l2sink->queue);
            sc_mutex_unlock(&v4l2sink->mutex);
            break;
        }

    }

    LOGD("V4l2sink thread ended");

    return 0;
}

bool
v4l2sink_start(struct v4l2sink *v4l2sink) {
    LOGD("Starting v4l2sink thread");

    bool ok = sc_thread_create(&v4l2sink->thread, run_v4l2sink, "v4l2sink",
                               v4l2sink);
    if (!ok) {
        LOGC("Could not start v4l2sink thread");
        return false;
    }

    return true;
}

void
v4l2sink_stop(struct v4l2sink *v4l2sink) {
    sc_mutex_lock(&v4l2sink->mutex);
    v4l2sink->stopped = true;
    sc_cond_signal(&v4l2sink->queue_cond);
    sc_mutex_unlock(&v4l2sink->mutex);
}

void
v4l2sink_join(struct v4l2sink *v4l2sink) {
    sc_thread_join(&v4l2sink->thread, NULL);
}

bool
v4l2sink_push(struct v4l2sink *v4l2sink, const AVPacket *packet) {
// the new decoding/encoding API has been introduced by:
// <http://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=7fc329e2dd6226dfecaa4a1d7adf353bf2773726>
#ifdef SCRCPY_LAVF_HAS_NEW_ENCODING_DECODING_API
    int ret;
    if ((ret = avcodec_send_packet(v4l2sink->decoder_ctx, packet)) < 0) {
        LOGE("Could not send video packet x: %d", ret);
        return false;
    }

    if ((ret = avcodec_receive_frame(v4l2sink->decoder_ctx, v4l2sink->decoded_frame)) < 0) {
        LOGE("Could not receive video frame: %d", ret);
        return false;
    }

    if ((ret = avcodec_send_frame(v4l2sink->encoder_ctx, v4l2sink->decoded_frame)) < 0) {
        LOGE("Could not send video frame: %d", ret);
        return true;
    }

    if ((ret = avcodec_receive_packet(v4l2sink->encoder_ctx, v4l2sink->raw_packet)) < 0) {
        LOGE("Could not receive video packet: %d", ret);
        return false;
    }
#else
    int got_picture;
    int len = avcodec_decode_video2(v4l2sink->decoder_ctx,
                                    v4l2sink->decoded_frame,
                                    &got_picture,
                                    packet);
    if (len < 0) {
        LOGE("Could not decode video packet: %d", len);
        return false;
    }
    if (!got_picture) {
        return false;
    }

    len = avcodec_encode_video2(v4l2sink->encoder_ctx,
                                v4l2sink->raw_packet,
                                v4l2sink->decoded_frame,
                                &got_picture);
    if (len < 0) {
        LOGE("Could not encode video packet: %d", len);
        return false;
    }
    if (!got_picture) {
        return false;
    }
#endif

    sc_mutex_lock(&v4l2sink->mutex);
    assert(!v4l2sink->stopped);

    if (v4l2sink->failed) {
        // reject any new packet (this will stop the stream)
        sc_mutex_unlock(&v4l2sink->mutex);
        return false;
    }

    struct v4l2sink_packet *rec = v4l2sink_packet_new(v4l2sink->raw_packet);
    if (!rec) {
        LOGC("Could not allocate v4l2sink packet");
        sc_mutex_unlock(&v4l2sink->mutex);
        return false;
    }

    queue_push(&v4l2sink->queue, next, rec);
    sc_cond_signal(&v4l2sink->queue_cond);

    sc_mutex_unlock(&v4l2sink->mutex);
    return true;
}

#endif
