#include "recorder.h"

#include <assert.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

#include "util/log.h"
#include "util/str.h"

/** Downcast packet_sink to recorder */
#define DOWNCAST(SINK) container_of(SINK, struct sc_recorder, packet_sink)

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
sc_recorder_write_header(struct sc_recorder *recorder, const AVPacket *packet) {
    AVStream *ostream = recorder->ctx->streams[0];

    uint8_t *extradata = av_malloc(packet->size * sizeof(uint8_t));
    if (!extradata) {
        LOG_OOM();
        return false;
    }

    // copy the first packet to the extra data
    memcpy(extradata, packet->data, packet->size);

    ostream->codecpar->extradata = extradata;
    ostream->codecpar->extradata_size = packet->size;

    int ret = avformat_write_header(recorder->ctx, NULL);
    if (ret < 0) {
        LOGE("Failed to write header to %s", recorder->filename);
        return false;
    }

    return true;
}

static void
sc_recorder_rescale_packet(struct sc_recorder *recorder, AVPacket *packet) {
    AVStream *ostream = recorder->ctx->streams[0];
    av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, ostream->time_base);
}

static bool
sc_recorder_write(struct sc_recorder *recorder, AVPacket *packet) {
    if (!recorder->header_written) {
        if (packet->pts != AV_NOPTS_VALUE) {
            LOGE("The first packet is not a config packet");
            return false;
        }
        bool ok = sc_recorder_write_header(recorder, packet);
        if (!ok) {
            return false;
        }
        recorder->header_written = true;
        return true;
    }

    if (packet->pts == AV_NOPTS_VALUE) {
        // ignore config packets
        return true;
    }

    sc_recorder_rescale_packet(recorder, packet);
    return av_write_frame(recorder->ctx, packet) >= 0;
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
    while (!recorder->codec && !recorder->stopped) {
        sc_cond_wait(&recorder->stream_cond, &recorder->mutex);
    }
    const AVCodec *codec = recorder->codec;
    sc_mutex_unlock(&recorder->mutex);

    if (codec) {
        AVStream *ostream = avformat_new_stream(recorder->ctx, codec);
        if (!ostream) {
            return false;
        }

        ostream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        ostream->codecpar->codec_id = codec->id;
        ostream->codecpar->format = AV_PIX_FMT_YUV420P;
        ostream->codecpar->width = recorder->declared_frame_size.width;
        ostream->codecpar->height = recorder->declared_frame_size.height;
    }

    return true;
}

static bool
sc_recorder_process_packets(struct sc_recorder *recorder) {
    int64_t pts_origin = AV_NOPTS_VALUE;

    // We can write a packet only once we received the next one so that we can
    // set its duration (next_pts - current_pts)
    struct sc_record_packet *previous = NULL;

    for (;;) {
        sc_mutex_lock(&recorder->mutex);

        while (!recorder->stopped && sc_queue_is_empty(&recorder->queue)) {
            sc_cond_wait(&recorder->queue_cond, &recorder->mutex);
        }

        // if stopped is set, continue to process the remaining events (to
        // finish the recording) before actually stopping

        if (recorder->stopped && sc_queue_is_empty(&recorder->queue)) {
            sc_mutex_unlock(&recorder->mutex);
            struct sc_record_packet *last = previous;
            if (last) {
                // assign an arbitrary duration to the last packet
                last->packet->duration = 100000;
                bool ok = sc_recorder_write(recorder, last->packet);
                if (!ok) {
                    // failing to write the last frame is not very serious, no
                    // future frame may depend on it, so the resulting file
                    // will still be valid
                    LOGW("Could not record last packet");
                }
                sc_record_packet_delete(last);
            }
            break;
        }

        struct sc_record_packet *rec;
        sc_queue_take(&recorder->queue, next, &rec);

        sc_mutex_unlock(&recorder->mutex);

        if (pts_origin == AV_NOPTS_VALUE
                && rec->packet->pts != AV_NOPTS_VALUE) {
            // First PTS received
            pts_origin = rec->packet->pts;
        }

        if (rec->packet->pts != AV_NOPTS_VALUE) {
            // Set PTS relatve to the origin
            rec->packet->pts -= pts_origin;
            rec->packet->dts = rec->packet->pts;
        }

        if (!previous) {
            // we just received the first packet
            previous = rec;
            continue;
        }

        // config packets have no PTS, we must ignore them
        if (rec->packet->pts != AV_NOPTS_VALUE
            && previous->packet->pts != AV_NOPTS_VALUE) {
            // we now know the duration of the previous packet
            previous->packet->duration =
                rec->packet->pts - previous->packet->pts;
        }

        bool ok = sc_recorder_write(recorder, previous->packet);
        sc_record_packet_delete(previous);
        if (!ok) {
            LOGE("Could not record packet");

            return false;
        }

        previous = rec;
    }

    if (!recorder->header_written) {
        // the recorded file is empty
        return false;
    }

    int ret = av_write_trailer(recorder->ctx);
    if (ret < 0) {
        LOGE("Failed to write trailer to %s", recorder->filename);
        return false;
    }

    return true;
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
    sc_recorder_queue_clear(&recorder->queue);
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
sc_recorder_packet_sink_open(struct sc_packet_sink *sink,
                             const AVCodec *codec) {
    struct sc_recorder *recorder = DOWNCAST(sink);
    assert(codec);

    sc_mutex_lock(&recorder->mutex);
    if (recorder->stopped) {
        sc_mutex_unlock(&recorder->mutex);
        return false;
    }

    recorder->codec = codec;
    sc_cond_signal(&recorder->stream_cond);
    sc_mutex_unlock(&recorder->mutex);

    return true;
}

static void
sc_recorder_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_recorder *recorder = DOWNCAST(sink);

    sc_mutex_lock(&recorder->mutex);
    // EOS also stops the recorder
    recorder->stopped = true;
    sc_cond_signal(&recorder->queue_cond);
    sc_mutex_unlock(&recorder->mutex);
}

static bool
sc_recorder_packet_sink_push(struct sc_packet_sink *sink,
                             const AVPacket *packet) {
    struct sc_recorder *recorder = DOWNCAST(sink);

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

    sc_queue_push(&recorder->queue, next, rec);
    sc_cond_signal(&recorder->queue_cond);

    sc_mutex_unlock(&recorder->mutex);
    return true;
}

bool
sc_recorder_init(struct sc_recorder *recorder, const char *filename,
                 enum sc_record_format format,
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

    sc_queue_init(&recorder->queue);
    recorder->stopped = false;
    recorder->header_written = false;

    recorder->codec = NULL;

    recorder->format = format;
    recorder->declared_frame_size = declared_frame_size;

    assert(cbs && cbs->on_ended);
    recorder->cbs = cbs;
    recorder->cbs_userdata = cbs_userdata;

    static const struct sc_packet_sink_ops ops = {
        .open = sc_recorder_packet_sink_open,
        .close = sc_recorder_packet_sink_close,
        .push = sc_recorder_packet_sink_push,
    };

    recorder->packet_sink.ops = &ops;

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
