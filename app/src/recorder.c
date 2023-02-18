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

static struct sc_record_packet *
sc_record_packet_new(const AVPacket *packet) {
    struct sc_record_packet *rec = malloc(sizeof(*rec));
    if (!rec) {
        LOG_OOM();
        return NULL;
    }

    rec->packet = av_packet_alloc();
    if (!rec->packet) {
        LOG_OOM();
        free(rec);
        return NULL;
    }

    if (av_packet_ref(rec->packet, packet)) {
        av_packet_free(&rec->packet);
        free(rec);
        return NULL;
    }
    return rec;
}

static void
sc_record_packet_delete(struct sc_record_packet *rec) {
    av_packet_free(&rec->packet);
    free(rec);
}

static void
sc_recorder_queue_clear(struct sc_recorder_queue *queue) {
    while (!sc_queue_is_empty(queue)) {
        struct sc_record_packet *rec;
        sc_queue_take(queue, next, &rec);
        sc_record_packet_delete(rec);
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

static bool
sc_recorder_wait_video_stream(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);
    while (!recorder->video_codec && !recorder->stopped) {
        sc_cond_wait(&recorder->stream_cond, &recorder->mutex);
    }
    const AVCodec *codec = recorder->video_codec;
    sc_mutex_unlock(&recorder->mutex);

    if (codec) {
        AVStream *stream = avformat_new_stream(recorder->ctx, codec);
        if (!stream) {
            return false;
        }

        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->codecpar->codec_id = codec->id;
        stream->codecpar->format = AV_PIX_FMT_YUV420P;
        stream->codecpar->width = recorder->declared_frame_size.width;
        stream->codecpar->height = recorder->declared_frame_size.height;

        recorder->video_stream_index = stream->index;
    }

    return true;
}

static bool
sc_recorder_wait_audio_stream(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);
    while (!recorder->audio_codec && !recorder->stopped) {
        sc_cond_wait(&recorder->stream_cond, &recorder->mutex);
    }
    const AVCodec *codec = recorder->audio_codec;
    sc_mutex_unlock(&recorder->mutex);

    if (codec) {
        AVStream *stream = avformat_new_stream(recorder->ctx, codec);
        if (!stream) {
            return false;
        }

        stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        stream->codecpar->codec_id = codec->id;
        stream->codecpar->ch_layout.nb_channels = 2;
        stream->codecpar->sample_rate = 48000;

        recorder->audio_stream_index = stream->index;
    }

    return true;
}

static inline bool
sc_recorder_has_empty_queues(struct sc_recorder *recorder) {
    if (sc_queue_is_empty(&recorder->video_queue)) {
        // The video queue is empty
        return true;
    }

    if (recorder->audio && sc_queue_is_empty(&recorder->audio_queue)) {
        // The audio queue is empty (when audio is enabled)
        return true;
    }

    // No queue is empty
    return false;
}

static bool
sc_recorder_process_header(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);

    while (!recorder->stopped && sc_recorder_has_empty_queues(recorder)) {
        sc_cond_wait(&recorder->queue_cond, &recorder->mutex);
    }

    if (sc_recorder_has_empty_queues(recorder)) {
        assert(recorder->stopped);
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    struct sc_record_packet *video_pkt;
    sc_queue_take(&recorder->video_queue, next, &video_pkt);

    struct sc_record_packet *audio_pkt;
    if (recorder->audio) {
        sc_queue_take(&recorder->audio_queue, next, &audio_pkt);
    }

    sc_mutex_unlock(&recorder->mutex);

    int ret = false;

    if (video_pkt->packet->pts != AV_NOPTS_VALUE) {
        LOGE("The first video packet is not a config packet");
        goto end;
    }

    assert(recorder->video_stream_index >= 0);
    AVStream *video_stream =
        recorder->ctx->streams[recorder->video_stream_index];
    bool ok = sc_recorder_set_extradata(video_stream, video_pkt->packet);
    if (!ok) {
        goto end;
    }

    if (recorder->audio) {
        if (audio_pkt->packet->pts != AV_NOPTS_VALUE) {
            LOGE("The first audio packet is not a config packet");
            goto end;
        }

        assert(recorder->audio_stream_index >= 0);
        AVStream *audio_stream =
            recorder->ctx->streams[recorder->audio_stream_index];
        ok = sc_recorder_set_extradata(audio_stream, audio_pkt->packet);
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
    sc_record_packet_delete(video_pkt);
    if (recorder->audio) {
        sc_record_packet_delete(audio_pkt);
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

    struct sc_record_packet *video_pkt = NULL;
    struct sc_record_packet *audio_pkt = NULL;

    // We can write a video packet only once we received the next one so that
    // we can set its duration (next_pts - current_pts)
    struct sc_record_packet *video_pkt_previous = NULL;

    bool error = false;

    for (;;) {
        sc_mutex_lock(&recorder->mutex);

        while (!recorder->stopped) {
            if (!video_pkt && !sc_queue_is_empty(&recorder->video_queue)) {
                // A new packet may be assigned to video_pkt and be processed
                break;
            }
            if (recorder->audio && !audio_pkt
                    && !sc_queue_is_empty(&recorder->audio_queue)) {
                // A new packet may be assigned to audio_pkt and be processed
                break;
            }
            sc_cond_wait(&recorder->queue_cond, &recorder->mutex);
        }

        // If stopped is set, continue to process the remaining events (to
        // finish the recording) before actually stopping.

        // If there is no audio, then the audio_queue will remain empty forever
        // and audio_pkt will always be NULL.
        assert(recorder->audio
                || (!audio_pkt && sc_queue_is_empty(&recorder->audio_queue)));

        if (!video_pkt && !sc_queue_is_empty(&recorder->video_queue)) {
            sc_queue_take(&recorder->video_queue, next, &video_pkt);
        }

        if (!audio_pkt && !sc_queue_is_empty(&recorder->audio_queue)) {
            sc_queue_take(&recorder->audio_queue, next, &audio_pkt);
        }

        if (recorder->stopped && !video_pkt && !audio_pkt) {
            assert(sc_queue_is_empty(&recorder->video_queue));
            assert(sc_queue_is_empty(&recorder->audio_queue));
            sc_mutex_unlock(&recorder->mutex);
            break;
        }

        assert(video_pkt || audio_pkt); // at least one

        sc_mutex_unlock(&recorder->mutex);

        // Ignore further config packets (e.g. on device orientation
        // change). The next non-config packet will have the config packet
        // data prepended.
        if (video_pkt && video_pkt->packet->pts == AV_NOPTS_VALUE) {
            sc_record_packet_delete(video_pkt);
            video_pkt = NULL;
        }

        if (audio_pkt && audio_pkt->packet->pts == AV_NOPTS_VALUE) {
            sc_record_packet_delete(audio_pkt);
            audio_pkt= NULL;
        }

        if (pts_origin == AV_NOPTS_VALUE) {
            if (!recorder->audio) {
                assert(video_pkt);
                pts_origin = video_pkt->packet->pts;
            } else if (video_pkt && audio_pkt) {
                pts_origin =
                    MIN(video_pkt->packet->pts, audio_pkt->packet->pts);
            } else {
                // We need both video and audio packets to initialize pts_origin
                continue;
            }
        }

        assert(pts_origin != AV_NOPTS_VALUE);

        if (video_pkt) {
            video_pkt->packet->pts -= pts_origin;
            video_pkt->packet->dts = video_pkt->packet->pts;

            if (video_pkt_previous) {
                // we now know the duration of the previous packet
                video_pkt_previous->packet->duration =
                    video_pkt->packet->pts - video_pkt_previous->packet->pts;

                bool ok = sc_recorder_write_video(recorder,
                                                  video_pkt_previous->packet);
                sc_record_packet_delete(video_pkt_previous);
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
            audio_pkt->packet->pts -= pts_origin;
            audio_pkt->packet->dts = audio_pkt->packet->pts;

            bool ok = sc_recorder_write_audio(recorder, audio_pkt->packet);
            if (!ok) {
                LOGE("Could not record audio packet");
                error = true;
                goto end;
            }

            sc_record_packet_delete(audio_pkt);
            audio_pkt = NULL;
        }
    }

    // Write the last video packet
    struct sc_record_packet *last = video_pkt_previous;
    if (last) {
        // assign an arbitrary duration to the last packet
        last->packet->duration = 100000;
        bool ok = sc_recorder_write_video(recorder, last->packet);
        if (!ok) {
            // failing to write the last frame is not very serious, no
            // future frame may depend on it, so the resulting file
            // will still be valid
            LOGW("Could not record last packet");
        }
        sc_record_packet_delete(last);
    }

    int ret = av_write_trailer(recorder->ctx);
    if (ret < 0) {
        LOGE("Failed to write trailer to %s", recorder->filename);
        error = false;
    }

end:
    if (video_pkt) {
        sc_record_packet_delete(video_pkt);
    }
    if (audio_pkt) {
        sc_record_packet_delete(audio_pkt);
    }

    return !error;
}

static bool
sc_recorder_record(struct sc_recorder *recorder) {
    bool ok = sc_recorder_open_output_file(recorder);
    if (!ok) {
        return false;
    }

    ok = sc_recorder_wait_video_stream(recorder);
    if (!ok) {
        sc_recorder_close_output_file(recorder);
        return false;
    }

    if (recorder->audio) {
        ok = sc_recorder_wait_audio_stream(recorder);
        if (!ok) {
            sc_recorder_close_output_file(recorder);
            return false;
        }
    }

    // If recorder->stopped, process any queued packet anyway

    ok = sc_recorder_process_packets(recorder);
    sc_recorder_close_output_file(recorder);
    return ok;
}

static int
run_recorder(void *data) {
    struct sc_recorder *recorder = data;

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
                                   const AVCodec *codec) {
    struct sc_recorder *recorder = DOWNCAST_VIDEO(sink);
    assert(codec);

    sc_mutex_lock(&recorder->mutex);
    if (recorder->stopped) {
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    recorder->video_codec = codec;
    sc_cond_signal(&recorder->stream_cond);
    sc_mutex_unlock(&recorder->mutex);

    return true;
}

static void
sc_recorder_video_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_recorder *recorder = DOWNCAST_VIDEO(sink);

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

    sc_mutex_lock(&recorder->mutex);

    if (recorder->stopped) {
        // reject any new packet
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    struct sc_record_packet *rec = sc_record_packet_new(packet);
    if (!rec) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    rec->packet->stream_index = recorder->video_stream_index;

    sc_queue_push(&recorder->video_queue, next, rec);
    sc_cond_signal(&recorder->queue_cond);

    sc_mutex_unlock(&recorder->mutex);
    return true;
}

static bool
sc_recorder_audio_packet_sink_open(struct sc_packet_sink *sink,
                                   const AVCodec *codec) {
    struct sc_recorder *recorder = DOWNCAST_AUDIO(sink);
    assert(recorder->audio);
    assert(codec);

    sc_mutex_lock(&recorder->mutex);
    recorder->audio_codec = codec;
    sc_cond_signal(&recorder->stream_cond);
    sc_mutex_unlock(&recorder->mutex);

    return true;
}

static void
sc_recorder_audio_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_recorder *recorder = DOWNCAST_AUDIO(sink);
    assert(recorder->audio);

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

    sc_mutex_lock(&recorder->mutex);

    if (recorder->stopped) {
        // reject any new packet
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    struct sc_record_packet *rec = sc_record_packet_new(packet);
    if (!rec) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    rec->packet->stream_index = recorder->audio_stream_index;

    sc_queue_push(&recorder->audio_queue, next, rec);
    sc_cond_signal(&recorder->queue_cond);

    sc_mutex_unlock(&recorder->mutex);
    return true;
}

bool
sc_recorder_init(struct sc_recorder *recorder, const char *filename,
                 enum sc_record_format format, bool audio,
                 struct sc_size declared_frame_size,
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

    sc_queue_init(&recorder->video_queue);
    sc_queue_init(&recorder->audio_queue);
    recorder->stopped = false;

    recorder->video_codec = NULL;
    recorder->audio_codec = NULL;

    recorder->video_stream_index = -1;
    recorder->audio_stream_index = -1;

    recorder->format = format;
    recorder->declared_frame_size = declared_frame_size;

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
