#include "stream_sink.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

#include "util/log.h"

/** Downcast packet sinks to stream sink */
#define DOWNCAST_VIDEO(SINK) \
    container_of(SINK, struct sc_stream_sink, video_packet_sink)
#define DOWNCAST_AUDIO(SINK) \
    container_of(SINK, struct sc_stream_sink, audio_packet_sink)

static const AVRational SCRCPY_TIME_BASE = {1, 1000000}; // timestamps in us

static AVPacket *
sc_stream_sink_packet_ref(const AVPacket *packet) {
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
sc_stream_sink_queue_clear(struct sc_stream_sink_queue *queue) {
    while (!sc_vecdeque_is_empty(queue)) {
        AVPacket *p = sc_vecdeque_pop(queue);
        av_packet_free(&p);
    }
}

static bool
sc_stream_sink_set_extradata(AVStream *ostream, const AVPacket *packet) {
    uint8_t *extradata = av_malloc(packet->size * sizeof(uint8_t));
    if (!extradata) {
        LOG_OOM();
        return false;
    }

    // copy the first packet to the extra data
    memcpy(extradata, packet->data, packet->size);

    ostream->codecpar->extradata = extradata;
    ostream->codecpar->extradata_size = packet->size;
    return true;
}

static inline void
sc_stream_sink_rescale_packet(AVStream *stream, AVPacket *packet) {
    av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, stream->time_base);
}

static bool
sc_stream_sink_write_stream(struct sc_stream_sink *sink,
                            struct sc_stream_sink_stream *st,
                            AVPacket *packet) {
    AVStream *stream = sink->ctx->streams[st->index];
    sc_stream_sink_rescale_packet(stream, packet);
    if (st->last_pts != AV_NOPTS_VALUE && packet->pts <= st->last_pts) {
        LOGD("Fixing PTS non monotonically increasing in stream %d "
             "(%" PRIi64 " >= %" PRIi64 ")",
             st->index, st->last_pts, packet->pts);
        packet->pts = ++st->last_pts;
        packet->dts = packet->pts;
    } else {
        st->last_pts = packet->pts;
    }
    return av_interleaved_write_frame(sink->ctx, packet) >= 0;
}

static inline bool
sc_stream_sink_write_video(struct sc_stream_sink *sink, AVPacket *packet) {
    return sc_stream_sink_write_stream(sink, &sink->video_stream, packet);
}

static inline bool
sc_stream_sink_write_audio(struct sc_stream_sink *sink, AVPacket *packet) {
    return sc_stream_sink_write_stream(sink, &sink->audio_stream, packet);
}

static int
sc_stream_sink_interrupt_cb(void *data) {
    struct sc_stream_sink *sink = data;
    // Read without mutex: this is intentional (same pattern as interrupt
    // callbacks in other parts of the codebase)
    return sink->stopped ? 1 : 0;
}

static inline bool
sc_stream_sink_must_wait_for_config_packets(struct sc_stream_sink *sink) {
    if (sink->video && sc_vecdeque_is_empty(&sink->video_queue)) {
        // The video queue is empty
        return true;
    }

    if (sink->audio && sink->audio_expects_config_packet
            && sc_vecdeque_is_empty(&sink->audio_queue)) {
        // The audio queue is empty (when audio is enabled)
        return true;
    }

    // No queue is empty
    return false;
}

static bool
sc_stream_sink_process_header(struct sc_stream_sink *sink) {
    sc_mutex_lock(&sink->mutex);

    while (!sink->stopped &&
              ((sink->video && !sink->video_init)
            || (sink->audio && !sink->audio_init)
            || sc_stream_sink_must_wait_for_config_packets(sink))) {
        sc_cond_wait(&sink->cond, &sink->mutex);
    }

    if (sink->video && sc_vecdeque_is_empty(&sink->video_queue)) {
        assert(sink->stopped);
        // If the stream sink is stopped, don't process anything if there are
        // not at least video packets
        sc_mutex_unlock(&sink->mutex);
        return false;
    }

    AVPacket *video_pkt = NULL;
    if (!sc_vecdeque_is_empty(&sink->video_queue)) {
        assert(sink->video);
        video_pkt = sc_vecdeque_pop(&sink->video_queue);
    }

    AVPacket *audio_pkt = NULL;
    if (sink->audio_expects_config_packet &&
            !sc_vecdeque_is_empty(&sink->audio_queue)) {
        assert(sink->audio);
        audio_pkt = sc_vecdeque_pop(&sink->audio_queue);
    }

    sc_mutex_unlock(&sink->mutex);

    bool ret = false;

    if (video_pkt) {
        if (video_pkt->pts != AV_NOPTS_VALUE) {
            LOGE("The first video packet is not a config packet");
            goto end;
        }

        assert(sink->video_stream.index >= 0);
        AVStream *video_stream = sink->ctx->streams[sink->video_stream.index];
        bool ok = sc_stream_sink_set_extradata(video_stream, video_pkt);
        if (!ok) {
            goto end;
        }
    }

    if (audio_pkt) {
        if (audio_pkt->pts != AV_NOPTS_VALUE) {
            LOGE("The first audio packet is not a config packet");
            goto end;
        }

        assert(sink->audio_stream.index >= 0);
        AVStream *audio_stream = sink->ctx->streams[sink->audio_stream.index];
        bool ok = sc_stream_sink_set_extradata(audio_stream, audio_pkt);
        if (!ok) {
            goto end;
        }
    }

    {
        // Open the TCP server: this blocks until a client connects (or
        // sink->stopped is set, via the interrupt callback)
        char url[64];
        snprintf(url, sizeof(url),
                 "tcp://0.0.0.0:%" PRIu16 "?listen=1", sink->port);

        AVIOInterruptCB int_cb = {
            .callback = sc_stream_sink_interrupt_cb,
            .opaque = sink,
        };

        LOGI("Stream sink: waiting for client on tcp://127.0.0.1:%" PRIu16,
             sink->port);

        int r = avio_open2(&sink->ctx->pb, url, AVIO_FLAG_WRITE,
                           &int_cb, NULL);
        if (r < 0) {
            if (!sink->stopped) {
                LOGE("Failed to open stream server on port %" PRIu16,
                     sink->port);
            }
            goto end;
        }
    }

    {
        bool ok = avformat_write_header(sink->ctx, NULL) >= 0;
        if (!ok) {
            LOGE("Failed to write stream header");
            avio_close(sink->ctx->pb);
            sink->ctx->pb = NULL;
            goto end;
        }
    }

    ret = true;

end:
    if (video_pkt) {
        av_packet_free(&video_pkt);
    }
    if (audio_pkt) {
        av_packet_free(&audio_pkt);
    }

    return ret;
}

static bool
sc_stream_sink_process_packets(struct sc_stream_sink *sink) {
    int64_t pts_origin = AV_NOPTS_VALUE;

    bool header_written = sc_stream_sink_process_header(sink);
    if (!header_written) {
        return false;
    }

    LOGI("Stream sink: streaming started on tcp://127.0.0.1:%" PRIu16,
         sink->port);

    AVPacket *video_pkt = NULL;
    AVPacket *audio_pkt = NULL;

    // We can write a video packet only once we received the next one so that
    // we can set its duration (next_pts - current_pts)
    AVPacket *video_pkt_previous = NULL;

    bool error = false;

    for (;;) {
        sc_mutex_lock(&sink->mutex);

        while (!sink->stopped) {
            if (sink->video && !video_pkt &&
                    !sc_vecdeque_is_empty(&sink->video_queue)) {
                // A new packet may be assigned to video_pkt and be processed
                break;
            }
            if (sink->audio && !audio_pkt
                    && !sc_vecdeque_is_empty(&sink->audio_queue)) {
                // A new packet may be assigned to audio_pkt and be processed
                break;
            }
            sc_cond_wait(&sink->cond, &sink->mutex);
        }

        // If stopped is set, continue to process the remaining events (to
        // finish the streaming) before actually stopping.

        // If there is no video, then the video_queue will remain empty forever
        // and video_pkt will always be NULL.
        assert(sink->video || (!video_pkt
                && sc_vecdeque_is_empty(&sink->video_queue)));

        // If there is no audio, then the audio_queue will remain empty forever
        // and audio_pkt will always be NULL.
        assert(sink->audio || (!audio_pkt
                && sc_vecdeque_is_empty(&sink->audio_queue)));

        if (!video_pkt && !sc_vecdeque_is_empty(&sink->video_queue)) {
            video_pkt = sc_vecdeque_pop(&sink->video_queue);
        }

        if (!audio_pkt && !sc_vecdeque_is_empty(&sink->audio_queue)) {
            audio_pkt = sc_vecdeque_pop(&sink->audio_queue);
        }

        if (sink->stopped && !video_pkt && !audio_pkt) {
            assert(sc_vecdeque_is_empty(&sink->video_queue));
            assert(sc_vecdeque_is_empty(&sink->audio_queue));
            sc_mutex_unlock(&sink->mutex);
            break;
        }

        assert(video_pkt || audio_pkt); // at least one

        sc_mutex_unlock(&sink->mutex);

        // Ignore further config packets (e.g. on device orientation
        // change). The next non-config packet will have the config packet
        // data prepended.
        if (video_pkt && video_pkt->pts == AV_NOPTS_VALUE) {
            av_packet_free(&video_pkt);
            video_pkt = NULL;
        }

        if (audio_pkt && audio_pkt->pts == AV_NOPTS_VALUE) {
            av_packet_free(&audio_pkt);
            audio_pkt = NULL;
        }

        if (pts_origin == AV_NOPTS_VALUE) {
            if (!sink->audio) {
                assert(video_pkt);
                pts_origin = video_pkt->pts;
            } else if (!sink->video) {
                assert(audio_pkt);
                pts_origin = audio_pkt->pts;
            } else if (video_pkt && audio_pkt) {
                pts_origin = MIN(video_pkt->pts, audio_pkt->pts);
            } else if (sink->stopped) {
                if (video_pkt) {
                    // The sink is stopped without audio, stream the video
                    // packets
                    pts_origin = video_pkt->pts;
                } else {
                    // Fail if there is no video
                    error = true;
                    goto end;
                }
            } else {
                // We need both video and audio packets to initialize pts_origin
                continue;
            }
        }

        assert(pts_origin != AV_NOPTS_VALUE);

        if (video_pkt) {
            video_pkt->pts -= pts_origin;
            video_pkt->dts = video_pkt->pts;

            if (video_pkt_previous) {
                // we now know the duration of the previous packet
                video_pkt_previous->duration = video_pkt->pts
                                             - video_pkt_previous->pts;

                bool ok = sc_stream_sink_write_video(sink, video_pkt_previous);
                av_packet_free(&video_pkt_previous);
                if (!ok) {
                    LOGE("Could not write video packet to stream");
                    error = true;
                    goto end;
                }
            }

            video_pkt_previous = video_pkt;
            video_pkt = NULL;
        }

        if (audio_pkt) {
            audio_pkt->pts -= pts_origin;
            audio_pkt->dts = audio_pkt->pts;

            bool ok = sc_stream_sink_write_audio(sink, audio_pkt);
            if (!ok) {
                LOGE("Could not write audio packet to stream");
                error = true;
                goto end;
            }

            av_packet_free(&audio_pkt);
            audio_pkt = NULL;
        }
    }

    // Write the last video packet
    AVPacket *last = video_pkt_previous;
    if (last) {
        // assign an arbitrary duration to the last packet: 0.1s in us
        last->duration = 100000;
        bool ok = sc_stream_sink_write_video(sink, last);
        if (!ok) {
            // failing to write the last frame is not very serious, no
            // future frame may depend on it, so the resulting stream
            // will still be valid
            LOGW("Could not write last packet to stream");
        }
        av_packet_free(&last);
        last = NULL;
    }

    av_write_trailer(sink->ctx);

end:
    if (video_pkt) {
        av_packet_free(&video_pkt);
    }
    if (audio_pkt) {
        av_packet_free(&audio_pkt);
    }
    if (video_pkt_previous) {
        av_packet_free(&video_pkt_previous);
    }

    return !error;
}

static int
run_stream_sink(void *data) {
    struct sc_stream_sink *sink = data;

    bool ok = sc_stream_sink_process_packets(sink);

    sc_mutex_lock(&sink->mutex);
    // Prevent the producer from pushing any new packet
    sink->stopped = true;
    // Discard pending packets
    sc_stream_sink_queue_clear(&sink->video_queue);
    sc_stream_sink_queue_clear(&sink->audio_queue);
    sc_mutex_unlock(&sink->mutex);

    if (sink->ctx->pb) {
        avio_close(sink->ctx->pb);
        sink->ctx->pb = NULL;
    }

    if (ok) {
        LOGI("Stream sink complete");
    } else {
        LOGE("Stream sink failed");
    }

    LOGD("Stream sink thread ended");

    return 0;
}

static void
sc_stream_sink_stream_init(struct sc_stream_sink_stream *stream) {
    stream->index = -1;
    stream->last_pts = AV_NOPTS_VALUE;
}

static bool
sc_stream_sink_video_packet_sink_open(struct sc_packet_sink *sink,
                                      AVCodecContext *ctx) {
    struct sc_stream_sink *ss = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(!ss->video_init);

    sc_mutex_lock(&ss->mutex);
    if (ss->stopped) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    AVStream *stream = avformat_new_stream(ss->ctx, ctx->codec);
    if (!stream) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    int r = avcodec_parameters_from_context(stream->codecpar, ctx);
    if (r < 0) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    ss->video_stream.index = stream->index;

    ss->video_init = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);

    return true;
}

static void
sc_stream_sink_video_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_stream_sink *ss = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(ss->video_init);

    sc_mutex_lock(&ss->mutex);
    // EOS also stops the stream sink
    ss->stopped = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);
}

static bool
sc_stream_sink_video_packet_sink_push(struct sc_packet_sink *sink,
                                      const AVPacket *packet) {
    struct sc_stream_sink *ss = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(ss->video_init);

    sc_mutex_lock(&ss->mutex);

    if (ss->stopped) {
        // reject any new packet
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    AVPacket *p = sc_stream_sink_packet_ref(packet);
    if (!p) {
        LOG_OOM();
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    p->stream_index = ss->video_stream.index;

    bool ok = sc_vecdeque_push(&ss->video_queue, p);
    if (!ok) {
        LOG_OOM();
        av_packet_free(&p);
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    sc_cond_signal(&ss->cond);

    sc_mutex_unlock(&ss->mutex);
    return true;
}

static bool
sc_stream_sink_audio_packet_sink_open(struct sc_packet_sink *sink,
                                      AVCodecContext *ctx) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(!ss->audio_init);

    sc_mutex_lock(&ss->mutex);

    AVStream *stream = avformat_new_stream(ss->ctx, ctx->codec);
    if (!stream) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    int r = avcodec_parameters_from_context(stream->codecpar, ctx);
    if (r < 0) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    ss->audio_stream.index = stream->index;

    // A config packet is provided for all supported formats except raw audio
    ss->audio_expects_config_packet =
        ctx->codec_id != AV_CODEC_ID_PCM_S16LE;

    ss->audio_init = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);

    return true;
}

static void
sc_stream_sink_audio_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(ss->audio_init);

    sc_mutex_lock(&ss->mutex);
    // EOS also stops the stream sink
    ss->stopped = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);
}

static bool
sc_stream_sink_audio_packet_sink_push(struct sc_packet_sink *sink,
                                      const AVPacket *packet) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(ss->audio_init);

    sc_mutex_lock(&ss->mutex);

    if (ss->stopped) {
        // reject any new packet
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    AVPacket *p = sc_stream_sink_packet_ref(packet);
    if (!p) {
        LOG_OOM();
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    p->stream_index = ss->audio_stream.index;

    bool ok = sc_vecdeque_push(&ss->audio_queue, p);
    if (!ok) {
        LOG_OOM();
        av_packet_free(&p);
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    sc_cond_signal(&ss->cond);

    sc_mutex_unlock(&ss->mutex);
    return true;
}

static void
sc_stream_sink_audio_packet_sink_disable(struct sc_packet_sink *sink) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(!ss->audio_init);

    LOGW("Audio stream disabled for stream sink");

    sc_mutex_lock(&ss->mutex);
    ss->audio = false;
    ss->audio_init = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);
}

bool
sc_stream_sink_init(struct sc_stream_sink *sink, uint16_t port,
                    bool video, bool audio) {
    assert(video || audio);

    sink->port = port;
    sink->video = video;
    sink->audio = audio;

    bool ok = sc_mutex_init(&sink->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&sink->cond);
    if (!ok) {
        goto error_mutex_destroy;
    }

    sink->stopped = false;

    sc_vecdeque_init(&sink->video_queue);
    sc_vecdeque_init(&sink->audio_queue);

    sink->video_init = false;
    sink->audio_init = false;

    sink->audio_expects_config_packet = false;

    sc_stream_sink_stream_init(&sink->video_stream);
    sc_stream_sink_stream_init(&sink->audio_stream);

    // Allocate the output format context with mpegts (ideal for TCP streaming)
    const AVOutputFormat *oformat = av_guess_format("mpegts", NULL, NULL);
    if (!oformat) {
        LOGE("Could not find mpegts muxer");
        goto error_cond_destroy;
    }

    sink->ctx = avformat_alloc_context();
    if (!sink->ctx) {
        LOG_OOM();
        goto error_cond_destroy;
    }

    // contrary to the deprecated API (av_oformat_next()), av_muxer_iterate()
    // returns (on purpose) a pointer-to-const, but AVFormatContext.oformat
    // still expects a pointer-to-non-const (it has not been updated accordingly)
    // <https://github.com/FFmpeg/FFmpeg/commit/0694d8702421e7aff1340038559c438b61bb30dd>
    sink->ctx->oformat = (AVOutputFormat *) oformat;

    if (video) {
        static const struct sc_packet_sink_ops video_ops = {
            .open = sc_stream_sink_video_packet_sink_open,
            .close = sc_stream_sink_video_packet_sink_close,
            .push = sc_stream_sink_video_packet_sink_push,
        };

        sink->video_packet_sink.ops = &video_ops;
    }

    if (audio) {
        static const struct sc_packet_sink_ops audio_ops = {
            .open = sc_stream_sink_audio_packet_sink_open,
            .close = sc_stream_sink_audio_packet_sink_close,
            .push = sc_stream_sink_audio_packet_sink_push,
            .disable = sc_stream_sink_audio_packet_sink_disable,
        };

        sink->audio_packet_sink.ops = &audio_ops;
    }

    return true;

error_cond_destroy:
    sc_cond_destroy(&sink->cond);
error_mutex_destroy:
    sc_mutex_destroy(&sink->mutex);

    return false;
}

bool
sc_stream_sink_start(struct sc_stream_sink *sink) {
    bool ok = sc_thread_create(&sink->thread, run_stream_sink,
                               "scrcpy-stream-sink", sink);
    if (!ok) {
        LOGE("Could not start stream sink thread");
        return false;
    }

    return true;
}

void
sc_stream_sink_stop(struct sc_stream_sink *sink) {
    sc_mutex_lock(&sink->mutex);
    sink->stopped = true;
    sc_cond_signal(&sink->cond);
    sc_mutex_unlock(&sink->mutex);
}

void
sc_stream_sink_join(struct sc_stream_sink *sink) {
    sc_thread_join(&sink->thread, NULL);
}

void
sc_stream_sink_destroy(struct sc_stream_sink *sink) {
    sc_cond_destroy(&sink->cond);
    sc_mutex_destroy(&sink->mutex);
    avformat_free_context(sink->ctx);
}
