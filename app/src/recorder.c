#include "recorder.h"

#include <assert.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

#include "util/log.h"
#include "util/str.h"

/** Downcast packet sinks to recorder */
#define DOWNCAST_VIDEO(SINK) \
    container_of(SINK, struct sc_recorder, video_packet_sink)
#define DOWNCAST_AUDIO(SINK) \
    container_of(SINK, struct sc_recorder, audio_packet_sink)

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
        // until null or containing the requested name
    } while (oformat && !sc_str_list_contains(oformat->name, ',', name));
    return oformat;
}

static AVPacket *
sc_recorder_packet_ref(const AVPacket *packet) {
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
sc_recorder_queue_clear(struct sc_recorder_queue *queue) {
    while (!sc_vecdeque_is_empty(queue)) {
        AVPacket *p = sc_vecdeque_pop(queue);
        av_packet_free(&p);
    }
}

static const char *
sc_recorder_get_format_name(enum sc_record_format format) {
    switch (format) {
        case SC_RECORD_FORMAT_MP4: return "mp4";
        case SC_RECORD_FORMAT_MKV: return "matroska";
        default: return NULL;
    }
}

static bool
sc_recorder_set_extradata(AVStream *ostream, const AVPacket *packet) {
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
sc_recorder_rescale_packet(AVStream *stream, AVPacket *packet) {
    av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, stream->time_base);
}

static bool
sc_recorder_write_stream(struct sc_recorder *recorder, int stream_index,
                         AVPacket *packet) {
    AVStream *stream = recorder->ctx->streams[stream_index];
    sc_recorder_rescale_packet(stream, packet);
    return av_interleaved_write_frame(recorder->ctx, packet) >= 0;
}

static inline bool
sc_recorder_write_video(struct sc_recorder *recorder, AVPacket *packet) {
    return sc_recorder_write_stream(recorder, recorder->video_stream_index,
                                    packet);
}

static inline bool
sc_recorder_write_audio(struct sc_recorder *recorder, AVPacket *packet) {
    return sc_recorder_write_stream(recorder, recorder->audio_stream_index,
                                    packet);
}

static bool
sc_recorder_open_output_file(struct sc_recorder *recorder) {
    const char *format_name = sc_recorder_get_format_name(recorder->format);
    assert(format_name);
    const AVOutputFormat *format = find_muxer(format_name);
    if (!format) {
        LOGE("Could not find muxer");
        return false;
    }

    recorder->ctx = avformat_alloc_context();
    if (!recorder->ctx) {
        LOG_OOM();
        return false;
    }

    int ret = avio_open(&recorder->ctx->pb, recorder->filename,
                        AVIO_FLAG_WRITE);
    if (ret < 0) {
        LOGE("Failed to open output file: %s", recorder->filename);
        avformat_free_context(recorder->ctx);
        return false;
    }

    // contrary to the deprecated API (av_oformat_next()), av_muxer_iterate()
    // returns (on purpose) a pointer-to-const, but AVFormatContext.oformat
    // still expects a pointer-to-non-const (it has not be updated accordingly)
    // <https://github.com/FFmpeg/FFmpeg/commit/0694d8702421e7aff1340038559c438b61bb30dd>
    recorder->ctx->oformat = (AVOutputFormat *) format;

    av_dict_set(&recorder->ctx->metadata, "comment",
                "Recorded by scrcpy " SCRCPY_VERSION, 0);

    LOGI("Recording started to %s file: %s", format_name, recorder->filename);
    return true;
}

static void
sc_recorder_close_output_file(struct sc_recorder *recorder) {
    avio_close(recorder->ctx->pb);
    avformat_free_context(recorder->ctx);
}

static inline bool
sc_recorder_has_empty_queues(struct sc_recorder *recorder) {
    if (sc_vecdeque_is_empty(&recorder->video_queue)) {
        // The video queue is empty
        return true;
    }

    if (recorder->audio && sc_vecdeque_is_empty(&recorder->audio_queue)) {
        // The audio queue is empty (when audio is enabled)
        return true;
    }

    // No queue is empty
    return false;
}

static bool
sc_recorder_process_header(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);

    while (!recorder->stopped && (!recorder->video_init
                               || !recorder->audio_init
                               || sc_recorder_has_empty_queues(recorder))) {
        sc_cond_wait(&recorder->stream_cond, &recorder->mutex);
    }

    if (sc_vecdeque_is_empty(&recorder->video_queue)) {
        assert(recorder->stopped);
        // If the recorder is stopped, don't process anything if there are not
        // at least video packets
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    AVPacket *video_pkt = sc_vecdeque_pop(&recorder->video_queue);

    AVPacket *audio_pkt = NULL;
    if (!sc_vecdeque_is_empty(&recorder->audio_queue)) {
        assert(recorder->audio);
        audio_pkt = sc_vecdeque_pop(&recorder->audio_queue);
    }

    sc_mutex_unlock(&recorder->mutex);

    int ret = false;

    if (video_pkt->pts != AV_NOPTS_VALUE) {
        LOGE("The first video packet is not a config packet");
        goto end;
    }

    assert(recorder->video_stream_index >= 0);
    AVStream *video_stream =
        recorder->ctx->streams[recorder->video_stream_index];
    bool ok = sc_recorder_set_extradata(video_stream, video_pkt);
    if (!ok) {
        goto end;
    }

    if (audio_pkt) {
        if (audio_pkt->pts != AV_NOPTS_VALUE) {
            LOGE("The first audio packet is not a config packet");
            goto end;
        }

        assert(recorder->audio_stream_index >= 0);
        AVStream *audio_stream =
            recorder->ctx->streams[recorder->audio_stream_index];
        ok = sc_recorder_set_extradata(audio_stream, audio_pkt);
        if (!ok) {
            goto end;
        }
    }

    ok = avformat_write_header(recorder->ctx, NULL) >= 0;
    if (!ok) {
        LOGE("Failed to write header to %s", recorder->filename);
        goto end;
    }

    ret = true;

end:
    av_packet_free(&video_pkt);
    if (audio_pkt) {
        av_packet_free(&audio_pkt);
    }

    return ret;
}

static bool
sc_recorder_process_packets(struct sc_recorder *recorder) {
    int64_t pts_origin = AV_NOPTS_VALUE;

    bool header_written = sc_recorder_process_header(recorder);
    if (!header_written) {
        return false;
    }

    AVPacket *video_pkt = NULL;
    AVPacket *audio_pkt = NULL;

    // We can write a video packet only once we received the next one so that
    // we can set its duration (next_pts - current_pts)
    AVPacket *video_pkt_previous = NULL;

    bool error = false;

    for (;;) {
        sc_mutex_lock(&recorder->mutex);

        while (!recorder->stopped) {
            if (!video_pkt && !sc_vecdeque_is_empty(&recorder->video_queue)) {
                // A new packet may be assigned to video_pkt and be processed
                break;
            }
            if (recorder->audio && !audio_pkt
                    && !sc_vecdeque_is_empty(&recorder->audio_queue)) {
                // A new packet may be assigned to audio_pkt and be processed
                break;
            }
            sc_cond_wait(&recorder->queue_cond, &recorder->mutex);
        }

        // If stopped is set, continue to process the remaining events (to
        // finish the recording) before actually stopping.

        // If there is no audio, then the audio_queue will remain empty forever
        // and audio_pkt will always be NULL.
        assert(recorder->audio || (!audio_pkt
                && sc_vecdeque_is_empty(&recorder->audio_queue)));

        if (!video_pkt && !sc_vecdeque_is_empty(&recorder->video_queue)) {
            video_pkt = sc_vecdeque_pop(&recorder->video_queue);
        }

        if (!audio_pkt && !sc_vecdeque_is_empty(&recorder->audio_queue)) {
            audio_pkt = sc_vecdeque_pop(&recorder->audio_queue);
        }

        if (recorder->stopped && !video_pkt && !audio_pkt) {
            assert(sc_vecdeque_is_empty(&recorder->video_queue));
            assert(sc_vecdeque_is_empty(&recorder->audio_queue));
            sc_mutex_unlock(&recorder->mutex);
            break;
        }

        assert(video_pkt || audio_pkt); // at least one

        sc_mutex_unlock(&recorder->mutex);

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
            if (!recorder->audio) {
                assert(video_pkt);
                pts_origin = video_pkt->pts;
            } else if (video_pkt && audio_pkt) {
                pts_origin = MIN(video_pkt->pts, audio_pkt->pts);
            } else if (recorder->stopped) {
                if (video_pkt) {
                    // The recorder is stopped without audio, record the video
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

                bool ok = sc_recorder_write_video(recorder, video_pkt_previous);
                av_packet_free(&video_pkt_previous);
                if (!ok) {
                    LOGE("Could not record video packet");
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

            bool ok = sc_recorder_write_audio(recorder, audio_pkt);
            if (!ok) {
                LOGE("Could not record audio packet");
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
        // assign an arbitrary duration to the last packet
        last->duration = 100000;
        bool ok = sc_recorder_write_video(recorder, last);
        if (!ok) {
            // failing to write the last frame is not very serious, no
            // future frame may depend on it, so the resulting file
            // will still be valid
            LOGW("Could not record last packet");
        }
        av_packet_free(&last);
    }

    int ret = av_write_trailer(recorder->ctx);
    if (ret < 0) {
        LOGE("Failed to write trailer to %s", recorder->filename);
        error = false;
    }

end:
    if (video_pkt) {
        av_packet_free(&video_pkt);
    }
    if (audio_pkt) {
        av_packet_free(&audio_pkt);
    }

    return !error;
}

static bool
sc_recorder_record(struct sc_recorder *recorder) {
    bool ok = sc_recorder_open_output_file(recorder);
    if (!ok) {
        return false;
    }

    ok = sc_recorder_process_packets(recorder);
    sc_recorder_close_output_file(recorder);
    return ok;
}

static int
run_recorder(void *data) {
    struct sc_recorder *recorder = data;

    // Recording is a background task
    bool ok = sc_thread_set_priority(SC_THREAD_PRIORITY_LOW);
    (void) ok; // We don't care if it worked

    bool success = sc_recorder_record(recorder);

    sc_mutex_lock(&recorder->mutex);
    // Prevent the producer to push any new packet
    recorder->stopped = true;
    // Discard pending packets
    sc_recorder_queue_clear(&recorder->video_queue);
    sc_recorder_queue_clear(&recorder->audio_queue);
    sc_mutex_unlock(&recorder->mutex);

    if (success) {
        const char *format_name = sc_recorder_get_format_name(recorder->format);
        LOGI("Recording complete to %s file: %s", format_name,
                                                  recorder->filename);
    } else {
        LOGE("Recording failed to %s", recorder->filename);
    }

    LOGD("Recorder thread ended");

    recorder->cbs->on_ended(recorder, success, recorder->cbs_userdata);

    return 0;
}

static bool
sc_recorder_video_packet_sink_open(struct sc_packet_sink *sink,
                                   AVCodecContext *ctx) {
    struct sc_recorder *recorder = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(!recorder->video_init);

    sc_mutex_lock(&recorder->mutex);
    if (recorder->stopped) {
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    AVStream *stream = avformat_new_stream(recorder->ctx, ctx->codec);
    if (!stream) {
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    int r = avcodec_parameters_from_context(stream->codecpar, ctx);
    if (r < 0) {
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    recorder->video_stream_index = stream->index;

    recorder->video_init = true;
    sc_cond_signal(&recorder->stream_cond);
    sc_mutex_unlock(&recorder->mutex);

    return true;
}

static void
sc_recorder_video_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_recorder *recorder = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(recorder->video_init);

    sc_mutex_lock(&recorder->mutex);
    // EOS also stops the recorder
    recorder->stopped = true;
    sc_cond_signal(&recorder->queue_cond);
    sc_mutex_unlock(&recorder->mutex);
}

static bool
sc_recorder_video_packet_sink_push(struct sc_packet_sink *sink,
                                   const AVPacket *packet) {
    struct sc_recorder *recorder = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(recorder->video_init);

    sc_mutex_lock(&recorder->mutex);

    if (recorder->stopped) {
        // reject any new packet
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    AVPacket *rec = sc_recorder_packet_ref(packet);
    if (!rec) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    rec->stream_index = recorder->video_stream_index;

    bool ok = sc_vecdeque_push(&recorder->video_queue, rec);
    if (!ok) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    sc_cond_signal(&recorder->queue_cond);

    sc_mutex_unlock(&recorder->mutex);
    return true;
}

static bool
sc_recorder_audio_packet_sink_open(struct sc_packet_sink *sink,
                                   AVCodecContext *ctx) {
    struct sc_recorder *recorder = DOWNCAST_AUDIO(sink);
    assert(recorder->audio);
    // only written from this thread, no need to lock
    assert(!recorder->audio_init);

    sc_mutex_lock(&recorder->mutex);

    AVStream *stream = avformat_new_stream(recorder->ctx, ctx->codec);
    if (!stream) {
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    int r = avcodec_parameters_from_context(stream->codecpar, ctx);
    if (r < 0) {
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    recorder->audio_stream_index = stream->index;

    recorder->audio_init = true;
    sc_cond_signal(&recorder->stream_cond);
    sc_mutex_unlock(&recorder->mutex);

    return true;
}

static void
sc_recorder_audio_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_recorder *recorder = DOWNCAST_AUDIO(sink);
    assert(recorder->audio);
    // only written from this thread, no need to lock
    assert(recorder->audio_init);

    sc_mutex_lock(&recorder->mutex);
    // EOS also stops the recorder
    recorder->stopped = true;
    sc_cond_signal(&recorder->queue_cond);
    sc_mutex_unlock(&recorder->mutex);
}

static bool
sc_recorder_audio_packet_sink_push(struct sc_packet_sink *sink,
                                   const AVPacket *packet) {
    struct sc_recorder *recorder = DOWNCAST_AUDIO(sink);
    assert(recorder->audio);
    // only written from this thread, no need to lock
    assert(recorder->audio_init);

    sc_mutex_lock(&recorder->mutex);

    if (recorder->stopped) {
        // reject any new packet
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    AVPacket *rec = sc_recorder_packet_ref(packet);
    if (!rec) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    rec->stream_index = recorder->audio_stream_index;

    bool ok = sc_vecdeque_push(&recorder->audio_queue, rec);
    if (!ok) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    sc_cond_signal(&recorder->queue_cond);

    sc_mutex_unlock(&recorder->mutex);
    return true;
}

static void
sc_recorder_audio_packet_sink_disable(struct sc_packet_sink *sink) {
    struct sc_recorder *recorder = DOWNCAST_AUDIO(sink);
    assert(recorder->audio);
    // only written from this thread, no need to lock
    assert(!recorder->audio_init);

    LOGW("Audio stream recording disabled");

    sc_mutex_lock(&recorder->mutex);
    recorder->audio = false;
    recorder->audio_init = true;
    sc_cond_signal(&recorder->stream_cond);
    sc_mutex_unlock(&recorder->mutex);
}

bool
sc_recorder_init(struct sc_recorder *recorder, const char *filename,
                 enum sc_record_format format, bool audio,
                 const struct sc_recorder_callbacks *cbs, void *cbs_userdata) {
    recorder->filename = strdup(filename);
    if (!recorder->filename) {
        LOG_OOM();
        return false;
    }

    bool ok = sc_mutex_init(&recorder->mutex);
    if (!ok) {
        goto error_free_filename;
    }

    ok = sc_cond_init(&recorder->queue_cond);
    if (!ok) {
        goto error_mutex_destroy;
    }

    ok = sc_cond_init(&recorder->stream_cond);
    if (!ok) {
        goto error_queue_cond_destroy;
    }

    recorder->audio = audio;

    sc_vecdeque_init(&recorder->video_queue);
    sc_vecdeque_init(&recorder->audio_queue);
    recorder->stopped = false;

    recorder->video_init = false;
    recorder->audio_init = false;

    recorder->video_stream_index = -1;
    recorder->audio_stream_index = -1;

    recorder->format = format;

    assert(cbs && cbs->on_ended);
    recorder->cbs = cbs;
    recorder->cbs_userdata = cbs_userdata;

    static const struct sc_packet_sink_ops video_ops = {
        .open = sc_recorder_video_packet_sink_open,
        .close = sc_recorder_video_packet_sink_close,
        .push = sc_recorder_video_packet_sink_push,
    };

    recorder->video_packet_sink.ops = &video_ops;

    if (audio) {
        static const struct sc_packet_sink_ops audio_ops = {
            .open = sc_recorder_audio_packet_sink_open,
            .close = sc_recorder_audio_packet_sink_close,
            .push = sc_recorder_audio_packet_sink_push,
            .disable = sc_recorder_audio_packet_sink_disable,
        };

        recorder->audio_packet_sink.ops = &audio_ops;
    }

    return true;

error_queue_cond_destroy:
    sc_cond_destroy(&recorder->queue_cond);
error_mutex_destroy:
    sc_mutex_destroy(&recorder->mutex);
error_free_filename:
    free(recorder->filename);

    return false;
}

bool
sc_recorder_start(struct sc_recorder *recorder) {
    bool ok = sc_thread_create(&recorder->thread, run_recorder,
                               "scrcpy-recorder", recorder);
    if (!ok) {
        LOGE("Could not start recorder thread");
        return false;
    }

    return true;
}

void
sc_recorder_stop(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);
    recorder->stopped = true;
    sc_cond_signal(&recorder->queue_cond);
    sc_cond_signal(&recorder->stream_cond);
    sc_mutex_unlock(&recorder->mutex);
}

void
sc_recorder_join(struct sc_recorder *recorder) {
    sc_thread_join(&recorder->thread, NULL);
}

void
sc_recorder_destroy(struct sc_recorder *recorder) {
    sc_cond_destroy(&recorder->stream_cond);
    sc_cond_destroy(&recorder->queue_cond);
    sc_mutex_destroy(&recorder->mutex);
    free(recorder->filename);
}
