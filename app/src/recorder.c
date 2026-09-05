#include "recorder.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavutil/display.h>

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
        case SC_RECORD_FORMAT_MP4:
        case SC_RECORD_FORMAT_M4A:
        case SC_RECORD_FORMAT_AAC:
            return "mp4";
        case SC_RECORD_FORMAT_MKV:
        case SC_RECORD_FORMAT_MKA:
            return "matroska";
        case SC_RECORD_FORMAT_WEBM:
            return "webm";
        case SC_RECORD_FORMAT_OPUS:
            return "opus";
        case SC_RECORD_FORMAT_FLAC:
            return "flac";
        case SC_RECORD_FORMAT_WAV:
            return "wav";
        default:
            return NULL;
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
sc_recorder_write_stream(struct sc_recorder *recorder,
                         struct sc_recorder_stream *st, AVPacket *packet) {
    AVStream *stream = recorder->ctx->streams[st->index];
    sc_recorder_rescale_packet(stream, packet);
    if (st->last_pts != AV_NOPTS_VALUE && packet->pts <= st->last_pts) {
        LOGD("Fixing PTS non monotonically increasing in stream %d "
             "(%" PRIi64 " >= %" PRIi64 ")",
             st->index, st->last_pts, packet->pts);
        packet->pts = ++st->last_pts;
        packet->dts = packet->pts;
    } else {
        st->last_pts = packet->pts;
    }
    return av_interleaved_write_frame(recorder->ctx, packet) >= 0;
}

static inline bool
sc_recorder_write_video(struct sc_recorder *recorder, AVPacket *packet) {
    return sc_recorder_write_stream(recorder, &recorder->video_stream, packet);
}

static inline bool
sc_recorder_write_audio(struct sc_recorder *recorder, AVPacket *packet) {
    return sc_recorder_write_stream(recorder, &recorder->audio_stream, packet);
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

    char *file_url = sc_str_concat("file:", recorder->filename);
    if (!file_url) {
        avformat_free_context(recorder->ctx);
        return false;
    }

    int ret = avio_open(&recorder->ctx->pb, file_url, AVIO_FLAG_WRITE);
    free(file_url);
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

static bool
sc_recorder_close_output_file(struct sc_recorder *recorder) {
    int ret = avio_close(recorder->ctx->pb);
    avformat_free_context(recorder->ctx);
    if (ret < 0) {
        LOGE("Failed to flush/close output file: %s", recorder->filename);
        return false;
    }
    return true;
}

static inline bool
sc_recorder_must_wait_for_config_packets(struct sc_recorder *recorder) {
    if (recorder->video && recorder->video_expects_config_packet
            && sc_vecdeque_is_empty(&recorder->video_queue)) {
        // The video queue is empty
        return true;
    }

    if (recorder->audio && recorder->audio_expects_config_packet
            && sc_vecdeque_is_empty(&recorder->audio_queue)) {
        // The audio queue is empty (when audio is enabled)
        return true;
    }

    // No queue is empty
    return false;
}

static bool
sc_recorder_process_header(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);

    while (!recorder->stopped &&
              ((recorder->video && !recorder->video_init)
            || (recorder->audio && !recorder->audio_init)
            || sc_recorder_must_wait_for_config_packets(recorder))) {
        sc_cond_wait(&recorder->cond, &recorder->mutex);
    }

    if (recorder->stopped && recorder->video
            && sc_vecdeque_is_empty(&recorder->video_queue)) {
        // If the recorder is stopped, don't process anything if there are not
        // at least video packets
        sc_mutex_unlock(&recorder->mutex);
        LOGW("Recording stopped before headers were processed");
        return false;
    }

    AVPacket *video_pkt = NULL;
    if (recorder->video_expects_config_packet &&
            !sc_vecdeque_is_empty(&recorder->video_queue)) {
        assert(recorder->video);
        video_pkt = sc_vecdeque_pop(&recorder->video_queue);
    }

    AVPacket *audio_pkt = NULL;
    if (recorder->audio_expects_config_packet &&
            !sc_vecdeque_is_empty(&recorder->audio_queue)) {
        assert(recorder->audio);
        audio_pkt = sc_vecdeque_pop(&recorder->audio_queue);
    }

    sc_mutex_unlock(&recorder->mutex);

    int ret = false;

    if (video_pkt) {
        if (video_pkt->pts != AV_NOPTS_VALUE) {
            LOGE("The first video packet is not a config packet");
            goto end;
        }

        assert(recorder->video_stream.index >= 0);
        AVStream *video_stream =
            recorder->ctx->streams[recorder->video_stream.index];
        bool ok = sc_recorder_set_extradata(video_stream, video_pkt);
        if (!ok) {
            goto end;
        }
    }

    if (audio_pkt) {
        if (audio_pkt->pts != AV_NOPTS_VALUE) {
            LOGE("The first audio packet is not a config packet");
            goto end;
        }

        assert(recorder->audio_stream.index >= 0);
        AVStream *audio_stream =
            recorder->ctx->streams[recorder->audio_stream.index];
        bool ok = sc_recorder_set_extradata(audio_stream, audio_pkt);
        if (!ok) {
            goto end;
        }
    }

    bool ok = avformat_write_header(recorder->ctx, NULL) >= 0;
    if (!ok) {
        LOGE("Failed to write header to %s", recorder->filename);
        goto end;
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
    AVPacket *video_config = NULL;

    bool error = false;

    for (;;) {
        sc_mutex_lock(&recorder->mutex);

        while (!recorder->stopped) {
            bool waiting_for_origin =
                recorder->video_pts_origin_required
                && recorder->video_pts_origin_us == AV_NOPTS_VALUE;
            if (!waiting_for_origin) {
                if (recorder->video && !video_pkt
                        && !sc_vecdeque_is_empty(&recorder->video_queue)) {
                    // A new packet may be assigned to video_pkt and be
                    // processed
                    break;
                }
                if (recorder->audio && !audio_pkt
                        && !sc_vecdeque_is_empty(&recorder->audio_queue)) {
                    // A new packet may be assigned to audio_pkt and be
                    // processed
                    break;
                }
            }
            sc_cond_wait(&recorder->cond, &recorder->mutex);
        }

        // If stopped is set, continue to process the remaining events (to
        // finish the recording) before actually stopping.

        // If there is no video, then the video_queue will remain empty forever
        // and video_pkt will always be NULL.
        assert(recorder->video || (!video_pkt
                && sc_vecdeque_is_empty(&recorder->video_queue)));

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

        if (recorder->video_pts_origin_required
                && recorder->video_pts_origin_us == AV_NOPTS_VALUE) {
            sc_mutex_unlock(&recorder->mutex);
            LOGE("Recording stopped before the decoded-frame origin was set");
            error = true;
            break;
        }

        int64_t forced_video_origin = recorder->video_pts_origin_us;

        assert(video_pkt || audio_pkt); // at least one

        sc_mutex_unlock(&recorder->mutex);

        // Ignore further config packets for H.26x: the demuxer prepends them
        // to the next media packet. AV1 config is not merged by the demuxer,
        // so retain it and prepend it here; otherwise a report after an
        // encoder reset keeps stale MP4 extradata and the new segment may be
        // undecodable.
        if (video_pkt && video_pkt->pts == AV_NOPTS_VALUE) {
            if (recorder->video_merges_late_config) {
                av_packet_free(&video_config);
                video_config = video_pkt;
            } else {
                av_packet_free(&video_pkt);
            }
            video_pkt = NULL;
        }

        if (audio_pkt && audio_pkt->pts == AV_NOPTS_VALUE) {
            av_packet_free(&audio_pkt);
            audio_pkt = NULL;
        }

        // A config-only iteration may happen while draining the queue after
        // stop. Continue so a following media packet can consume the retained
        // AV1 config instead of treating the config itself as missing video.
        if (!video_pkt && !audio_pkt) {
            continue;
        }

        if (video_pkt && forced_video_origin != AV_NOPTS_VALUE
                && video_pkt->pts < forced_video_origin) {
            // Decoder preroll before the first visible frame is not part of
            // the report timeline.
            av_packet_free(&video_pkt);
            continue;
        }

        if (pts_origin == AV_NOPTS_VALUE) {
            if (forced_video_origin != AV_NOPTS_VALUE && video_pkt) {
                if (video_pkt->pts != forced_video_origin) {
                    LOGE("First report packet PTS (%" PRId64
                         ") does not match first decoded-frame PTS (%" PRId64
                         ")", video_pkt->pts, forced_video_origin);
                    error = true;
                    goto end;
                }
                if (!(video_pkt->flags & AV_PKT_FLAG_KEY)) {
                    // A rare failure to retain the decoder's initial frame
                    // could otherwise move the chosen origin to a delta frame.
                    // Keeping pre-origin media would invent negative report
                    // time, while starting from that delta would produce a
                    // report which is not independently decodable.
                    LOGE("First report packet at the decoded-frame origin is "
                         "not a keyframe");
                    error = true;
                    goto end;
                }
                pts_origin = forced_video_origin;
            } else if (!recorder->audio && video_pkt) {
                pts_origin = video_pkt->pts;
            } else if (!recorder->video && audio_pkt) {
                pts_origin = audio_pkt->pts;
            } else if (recorder->video && recorder->audio && video_pkt
                    && audio_pkt) {
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
            if (video_config) {
                size_t config_size = video_config->size;
                size_t media_size = video_pkt->size;
                if (config_size > INT_MAX
                        || av_grow_packet(video_pkt, (int) config_size)) {
                    LOG_OOM();
                    error = true;
                    goto end;
                }
                memmove(video_pkt->data + config_size, video_pkt->data,
                        media_size);
                memcpy(video_pkt->data, video_config->data, config_size);
                av_packet_free(&video_config);
            }

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
    video_pkt_previous = NULL;
    sc_mutex_lock(&recorder->mutex);
    int64_t target = recorder->video_end_target_us;
    sc_mutex_unlock(&recorder->mutex);
    if (last) {
        // Normal recordings retain upstream's 100ms fallback. Auto-test
        // reports provide an exact session end so a static final frame spans
        // the whole report timeline instead of ending playback early.
        last->duration = target != AV_NOPTS_VALUE
                       ? (target > last->pts ? target - last->pts : 1)
                       : 100000;
        // sc_recorder_write_video() rescales the packet in place to the
        // container time base. Preserve the microsecond-domain duration for
        // report manifest consumers before that mutation.
        int64_t final_duration_us = last->pts + last->duration;
        bool ok = sc_recorder_write_video(recorder, last);
        if (!ok) {
            if (target != AV_NOPTS_VALUE) {
                // Reports promise that the media covers their exact timeline;
                // do not mark one finalized if its held tail was not written.
                LOGE("Could not record final report packet");
                error = true;
            } else {
                // Preserve upstream behavior for ordinary recordings.
                LOGW("Could not record last packet");
            }
        } else {
            recorder->video_duration_us = final_duration_us;
        }
        av_packet_free(&last);
    } else if (recorder->video && target != AV_NOPTS_VALUE) {
        LOGE("Could not finalize report: no video media packet was recorded");
        error = true;
    }

    int ret = av_write_trailer(recorder->ctx);
    if (ret < 0) {
        LOGE("Failed to write trailer to %s", recorder->filename);
        error = true;
    }

end:
    if (video_pkt) {
        av_packet_free(&video_pkt);
    }
    if (audio_pkt) {
        av_packet_free(&audio_pkt);
    }
    if (video_config) {
        av_packet_free(&video_config);
    }
    if (video_pkt_previous) {
        av_packet_free(&video_pkt_previous);
    }

    return !error;
}

static bool
sc_recorder_record(struct sc_recorder *recorder) {
    bool process_ok = sc_recorder_process_packets(recorder);
    bool close_ok = sc_recorder_close_output_file(recorder);
    return process_ok && close_ok;
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
sc_recorder_set_orientation(AVStream *stream, enum sc_orientation orientation) {
    assert(!sc_orientation_is_mirror(orientation));

    uint8_t *raw_data;
#ifdef SCRCPY_LAVC_HAS_CODECPAR_CODEC_SIDEDATA
    AVPacketSideData *sd =
        av_packet_side_data_new(&stream->codecpar->coded_side_data,
                                &stream->codecpar->nb_coded_side_data,
                                AV_PKT_DATA_DISPLAYMATRIX,
                                sizeof(int32_t) * 9, 0);
    if (!sd) {
        LOG_OOM();
        return false;
    }

    raw_data = sd->data;
#else
    raw_data = av_stream_new_side_data(stream, AV_PKT_DATA_DISPLAYMATRIX,
                                      sizeof(int32_t) * 9);
    if (!raw_data) {
        LOG_OOM();
        return false;
    }
#endif

    int32_t *matrix = (int32_t *) raw_data;

    unsigned rotation = orientation;
    unsigned angle = rotation * 90;

    av_display_rotation_set(matrix, angle);

    return true;
}

static bool
sc_recorder_video_packet_sink_open(struct sc_packet_sink *sink,
                                   AVCodecContext *ctx,
                                   const struct sc_stream_session *session) {
    (void) session;

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

    recorder->video_stream.index = stream->index;

    if (recorder->orientation != SC_ORIENTATION_0) {
        if (!sc_recorder_set_orientation(stream, recorder->orientation)) {
            sc_mutex_unlock(&recorder->mutex);
            return false;
        }

        LOGI("Record orientation set to %s",
             sc_orientation_get_name(recorder->orientation));
    }

    // A config packet is provided for all supported formats except VPx
    recorder->video_expects_config_packet = ctx->codec_id != AV_CODEC_ID_VP8
                                         && ctx->codec_id != AV_CODEC_ID_VP9;
#ifdef SCRCPY_LAVC_HAS_AV1
    recorder->video_merges_late_config = ctx->codec_id == AV_CODEC_ID_AV1;
#else
    recorder->video_merges_late_config = false;
#endif

    recorder->video_init = true;
    sc_cond_signal(&recorder->cond);
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
    sc_cond_signal(&recorder->cond);
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

    rec->stream_index = recorder->video_stream.index;

    bool ok = sc_vecdeque_push(&recorder->video_queue, rec);
    if (!ok) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    sc_cond_signal(&recorder->cond);

    sc_mutex_unlock(&recorder->mutex);
    return true;
}

static bool
sc_recorder_audio_packet_sink_open(struct sc_packet_sink *sink,
                                   AVCodecContext *ctx,
                                   const struct sc_stream_session *session) {
    (void) session;

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

    recorder->audio_stream.index = stream->index;

    // A config packet is provided for all supported formats except raw audio
    recorder->audio_expects_config_packet =
        ctx->codec_id != AV_CODEC_ID_PCM_S16LE;

    recorder->audio_init = true;
    sc_cond_signal(&recorder->cond);
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
    sc_cond_signal(&recorder->cond);
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

    rec->stream_index = recorder->audio_stream.index;

    bool ok = sc_vecdeque_push(&recorder->audio_queue, rec);
    if (!ok) {
        LOG_OOM();
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    sc_cond_signal(&recorder->cond);

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
    sc_cond_signal(&recorder->cond);
    sc_mutex_unlock(&recorder->mutex);
}

static void
sc_recorder_stream_init(struct sc_recorder_stream *stream) {
    stream->index = -1;
    stream->last_pts = AV_NOPTS_VALUE;
}

bool
sc_recorder_init(struct sc_recorder *recorder, const char *filename,
                 enum sc_record_format format, bool video, bool audio,
                 enum sc_orientation orientation,
                 const struct sc_recorder_callbacks *cbs, void *cbs_userdata) {
    assert(!sc_orientation_is_mirror(orientation));

    recorder->filename = strdup(filename);
    if (!recorder->filename) {
        LOG_OOM();
        return false;
    }

    bool ok = sc_mutex_init(&recorder->mutex);
    if (!ok) {
        goto error_free_filename;
    }

    ok = sc_cond_init(&recorder->cond);
    if (!ok) {
        goto error_mutex_destroy;
    }

    assert(video || audio);
    recorder->video = video;
    recorder->audio = audio;

    recorder->orientation = orientation;

    sc_vecdeque_init(&recorder->video_queue);
    sc_vecdeque_init(&recorder->audio_queue);
    recorder->stopped = false;

    recorder->video_init = false;
    recorder->audio_init = false;

    recorder->video_expects_config_packet = false;
    recorder->audio_expects_config_packet = false;
    recorder->video_merges_late_config = false;
    recorder->video_end_target_us = AV_NOPTS_VALUE;
    recorder->video_duration_us = 0;
    recorder->video_pts_origin_required = false;
    recorder->video_pts_origin_us = AV_NOPTS_VALUE;

    sc_recorder_stream_init(&recorder->video_stream);
    sc_recorder_stream_init(&recorder->audio_stream);

    recorder->format = format;

    assert(cbs && cbs->on_ended);
    recorder->cbs = cbs;
    recorder->cbs_userdata = cbs_userdata;

    if (video) {
        static const struct sc_packet_sink_ops video_ops = {
            .open = sc_recorder_video_packet_sink_open,
            .close = sc_recorder_video_packet_sink_close,
            .push = sc_recorder_video_packet_sink_push,
        };

        recorder->video_packet_sink.ops = &video_ops;
    }

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

error_mutex_destroy:
    sc_mutex_destroy(&recorder->mutex);
error_free_filename:
    free(recorder->filename);

    return false;
}

bool
sc_recorder_start(struct sc_recorder *recorder) {
    // Open synchronously before producers may call packet_sink.open(). This
    // publishes recorder->ctx before the demuxer thread can create streams and
    // removes the historical startup race with the recorder worker.
    if (!sc_recorder_open_output_file(recorder)) {
        return false;
    }

    bool ok = sc_thread_create(&recorder->thread, run_recorder,
                               "scrcpy-recorder", recorder);
    if (!ok) {
        LOGE("Could not start recorder thread");
        sc_recorder_close_output_file(recorder);
        return false;
    }

    return true;
}

void
sc_recorder_stop(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);
    recorder->stopped = true;
    sc_cond_signal(&recorder->cond);
    sc_mutex_unlock(&recorder->mutex);
}

void
sc_recorder_set_video_end(struct sc_recorder *recorder, int64_t end_us) {
    assert(end_us >= 0);
    sc_mutex_lock(&recorder->mutex);
    if (recorder->video_end_target_us == AV_NOPTS_VALUE
            || end_us > recorder->video_end_target_us) {
        recorder->video_end_target_us = end_us;
    }
    sc_mutex_unlock(&recorder->mutex);
}

void
sc_recorder_require_video_pts_origin(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);
    recorder->video_pts_origin_required = true;
    sc_mutex_unlock(&recorder->mutex);
}

void
sc_recorder_set_video_pts_origin(struct sc_recorder *recorder,
                                 int64_t origin_us) {
    assert(origin_us != AV_NOPTS_VALUE);
    sc_mutex_lock(&recorder->mutex);
    if (recorder->video_pts_origin_us == AV_NOPTS_VALUE) {
        recorder->video_pts_origin_us = origin_us;
        sc_cond_signal(&recorder->cond);
    } else {
        assert(recorder->video_pts_origin_us == origin_us);
    }
    sc_mutex_unlock(&recorder->mutex);
}

void
sc_recorder_join(struct sc_recorder *recorder) {
    sc_thread_join(&recorder->thread, NULL);
}

int64_t
sc_recorder_get_video_duration(struct sc_recorder *recorder) {
    sc_mutex_lock(&recorder->mutex);
    int64_t duration = recorder->video_duration_us;
    sc_mutex_unlock(&recorder->mutex);
    return duration;
}

void
sc_recorder_destroy(struct sc_recorder *recorder) {
    sc_cond_destroy(&recorder->cond);
    sc_mutex_destroy(&recorder->mutex);
    free(recorder->filename);
}
