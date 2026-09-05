#include "report.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
# include <direct.h>
#else
# include <sys/stat.h>
# include <sys/types.h>
#endif

#include "daemon/codec.h"
#include "daemon/clip_buffer.h"
#include "daemon/protocol.h" // sc_json_append_escaped
#include "util/log.h"
#include "util/strbuf.h"

#define DOWNCAST_VIDEO(SINK) \
    container_of(SINK, struct sc_report, video_packet_sink)

static bool
make_dir_one(const char *path) {
#ifdef _WIN32
    int r = _mkdir(path);
#else
    int r = mkdir(path, 0755);
#endif
    return r == 0 || errno == EEXIST;
}

// Create `path` and any missing parent directories (like `mkdir -p`), so a
// nested --auto-test-report target such as "./v/test1" works when "./v" does
// not exist yet. Leading and repeated slashes are preserved.
static bool
make_dir(const char *path) {
    char *copy = strdup(path);
    if (!copy) {
        LOG_OOM();
        return false;
    }
    bool ok = true;
    // Create each intermediate component, then the leaf. Skip the first
    // character so a leading '/' (absolute path) or '.' stays with the first
    // component, and an empty path never reads past the buffer.
    for (char *p = copy; *p; ++p) {
        if (p == copy || (*p != '/'
#ifdef _WIN32
                && *p != '\\'
#endif
        )) {
            continue;
        }
        char sep = *p;
        *p = '\0';
        if (*copy && !make_dir_one(copy)) { // skip empty (e.g. leading "//")
            ok = false;
            break;
        }
        *p = sep;
    }
    ok = ok && make_dir_one(copy);
    free(copy);
    return ok;
}

static char *
join(const char *dir, const char *name) {
    char *path;
    int r = asprintf(&path, "%s/%s", dir, name);
    if (r == -1) {
        LOG_OOM();
        return NULL;
    }
    return path;
}

// ISO8601 UTC with milliseconds into `out` (size >= 32)
static void
iso8601_now(char *out, size_t size) {
    time_t now = time(NULL);
    struct tm tm;
#ifdef _WIN32
    bool converted = gmtime_s(&tm, &now) == 0;
#else
    bool converted = gmtime_r(&now, &tm) != NULL;
#endif
    if (!converted
            || strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", &tm) == 0) {
        out[0] = '\0';
    }
}

void
sc_report_mark_failed(struct sc_report *report) {
    sc_mutex_lock(&report->mutex);
    report->recorder_failed = true;
    sc_mutex_unlock(&report->mutex);
}

static void
sc_report_on_recorder_ended(struct sc_recorder *recorder, bool success,
                            void *userdata) {
    (void) recorder;
    struct sc_report *report = userdata;
    if (!success) {
        sc_report_mark_failed(report);
        LOGE("Test report: recording failed");
    }
}

static bool
configure_video_format(struct sc_report *report, enum sc_codec codec) {
    report->video_codec = sc_daemon_codec_name(codec);
    if (!report->video_codec) {
        LOGE("Test report: unsupported video codec");
        return false;
    }

    // Match upstream scrcpy 4.1 recording constraints: VP8 cannot be muxed
    // into MP4. VP9 is supported in MP4.
    if (codec == SC_CODEC_VP8) {
        report->video_filename = "recording.webm";
        report->video_container = "webm";
        report->video_mime_type = "video/webm";
        report->video_format = SC_RECORD_FORMAT_WEBM;
    } else {
        report->video_filename = "recording.mp4";
        report->video_container = "mp4";
        report->video_mime_type = "video/mp4";
        report->video_format = SC_RECORD_FORMAT_MP4;
    }
    return true;
}

static bool
sc_report_video_packet_sink_open(
        struct sc_packet_sink *sink, AVCodecContext *ctx,
        const struct sc_stream_session *session) {
    struct sc_report *report = DOWNCAST_VIDEO(sink);
    struct sc_packet_sink *recorder_sink =
        &report->recorder.video_packet_sink;
    bool ok = recorder_sink->ops->open(recorder_sink, ctx, session);
    if (!ok) {
        sc_report_mark_failed(report);
    }
    return ok;
}

static bool
sc_report_publish_video_origin(struct sc_report *report) {
    int64_t origin;
    if (!report->keeper
            || !sc_frame_keeper_get_timeline_anchor(report->keeper, NULL,
                                                     &origin)) {
        return false;
    }

    sc_recorder_set_video_pts_origin(&report->recorder, origin);
    return true;
}

static void
sc_report_video_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_report *report = DOWNCAST_VIDEO(sink);

    if (!sc_report_publish_video_origin(report)) {
        LOGE("Test report: video stream closed before the first decoded-frame "
             "timeline anchor was established");
        sc_report_mark_failed(report);
    }

    // Do not close the recorder here. Packet sinks are closed in reverse order:
    // this wrapper closes before the decoder closes the frame keeper, and the
    // daemon does not close the event gate until after all in-flight requests
    // have drained. Stopping here could therefore finalize the video before the
    // keeper clock freezes (or before the final event is persisted). The
    // recorder has already received every encoded packet; report finalization
    // below publishes the frozen end target, then stops and joins it.
}

static bool
sc_report_video_packet_sink_push(struct sc_packet_sink *sink,
                                 const AVPacket *packet) {
    struct sc_report *report = DOWNCAST_VIDEO(sink);
    if (packet->pts != AV_NOPTS_VALUE) {
        // The decoded first frame may lag its encoded packet by several
        // packets. Queue them all in the recorder, but publish the immutable
        // decoded-frame media origin as soon as it becomes observable. The
        // recorder worker waits for this value and drops decoder preroll.
        sc_report_publish_video_origin(report);
    }
    struct sc_packet_sink *recorder_sink =
        &report->recorder.video_packet_sink;
    bool ok = recorder_sink->ops->push(recorder_sink, packet);
    if (!ok) {
        sc_report_mark_failed(report);
    }
    return ok;
}

static bool
sc_report_video_packet_sink_push_session(
        struct sc_packet_sink *sink,
        const struct sc_stream_session *session) {
    struct sc_report *report = DOWNCAST_VIDEO(sink);
    struct sc_packet_sink *recorder_sink =
        &report->recorder.video_packet_sink;
    bool ok = !recorder_sink->ops->push_session
           || recorder_sink->ops->push_session(recorder_sink, session);
    if (!ok) {
        sc_report_mark_failed(report);
    }
    return ok;
}

// Write manifest.json. `finalized` marks a clean end.
static bool
write_manifest(struct sc_report *report, bool finalized, int width, int height,
               int64_t duration_ms, const char *ended_at,
               uint64_t event_count) {
    char *path = join(report->dir, "manifest.json");
    if (!path) {
        return false;
    }
    char *tmp_path = join(report->dir, "manifest.json.tmp");
    if (!tmp_path) {
        free(path);
        return false;
    }

    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 512)) {
        free(tmp_path);
        free(path);
        return false;
    }

    char head[256];
    snprintf(head, sizeof(head),
             "{\"report_version\":1,\"app\":\"scrcpy-auto\",");
    bool w = sc_strbuf_append_str(&buf, head)
          && sc_strbuf_append_staticstr(&buf, "\"serial\":")
          && sc_json_append_escaped(&buf, report->serial)
          && sc_strbuf_append_staticstr(&buf, ",\"device_name\":")
          && sc_json_append_escaped(&buf, report->device_name);

    char tail[512];
    snprintf(tail, sizeof(tail),
             ",\"video\":{\"file\":\"%s\",\"codec\":\"%s\","
             "\"container\":\"%s\",\"mime_type\":\"%s\","
             "\"width\":%d,\"height\":%d,\"duration_ms\":%" PRId64 ","
             "\"finalized\":%s},\"ended_at\":%s%s%s,\"event_count\":%"
             PRIu64 "}\n",
             report->video_filename, report->video_codec,
             report->video_container, report->video_mime_type,
             width, height, duration_ms, finalized ? "true" : "false",
             ended_at ? "\"" : "null", ended_at ? ended_at : "",
             ended_at ? "\"" : "", event_count);
    w = w && sc_strbuf_append_str(&buf, tail);

    bool ok = false;
    if (w) {
        FILE *fp = fopen(tmp_path, "w");
        if (fp) {
            bool wrote = fwrite(buf.s, 1, buf.len, fp) == buf.len;
            bool flushed = fflush(fp) == 0;
            bool closed = fclose(fp) == 0;
            if (wrote && flushed && closed) {
#ifdef _WIN32
                // rename() does not replace an existing file on Windows.
                remove(path);
#endif
                ok = rename(tmp_path, path) == 0;
            }
        }
    }
    if (!ok) {
        LOGE("Test report: could not write manifest: %s", path);
        remove(tmp_path);
    }

    free(buf.s);
    free(tmp_path);
    free(path);
    return ok;
}

bool
sc_report_init(struct sc_report *report, const char *dir,
               struct sc_frame_keeper *keeper,
               struct sc_clip_buffer *timeline, const char *serial,
               const char *device_name, enum sc_codec video_codec) {
    if (!configure_video_format(report, video_codec)) {
        return false;
    }

    report->dir = strdup(dir);
    if (!report->dir) {
        LOG_OOM();
        return false;
    }

    if (!make_dir(report->dir)) {
        LOGE("Test report: could not create directory: %s", report->dir);
        goto error_free_dir;
    }

    report->video_path = join(report->dir, report->video_filename);
    if (!report->video_path) {
        goto error_free_dir;
    }

    if (!sc_mutex_init(&report->mutex)) {
        goto error_free_video;
    }

    char *events_path = join(report->dir, "events.jsonl");
    if (!events_path) {
        goto error_destroy_mutex;
    }
    report->events = fopen(events_path, "w");
    free(events_path);
    if (!report->events) {
        LOGE("Test report: could not open events log");
        goto error_destroy_mutex;
    }

    report->keeper = keeper;
    report->timeline = timeline;
    report->recorder_initialized = false;
    report->recorder_started = false;
    report->recorder_failed = false;
    report->io_failed = false;
    report->accepting_events = true;
    static const struct sc_packet_sink_ops video_ops = {
        .open = sc_report_video_packet_sink_open,
        .close = sc_report_video_packet_sink_close,
        .push = sc_report_video_packet_sink_push,
        .push_session = sc_report_video_packet_sink_push_session,
    };
    report->video_packet_sink.ops = &video_ops;
    report->seq = 0;
    snprintf(report->serial, sizeof(report->serial), "%s",
             serial ? serial : "");
    snprintf(report->device_name, sizeof(report->device_name), "%s",
             device_name ? device_name : "");

    if (!write_manifest(report, false, 0, 0, 0, NULL, 0)) {
        goto error_close_events;
    }

    LOGI("Test report: writing to %s", report->dir);
    return true;

error_close_events:
    fclose(report->events);
    report->events = NULL;
error_destroy_mutex:
    sc_mutex_destroy(&report->mutex);
error_free_video:
    free(report->video_path);
error_free_dir:
    free(report->dir);
    report->dir = NULL;
    return false;
}

bool
sc_report_start_recording(struct sc_report *report, bool video,
                          enum sc_orientation orientation) {
    static const struct sc_recorder_callbacks cbs = {
        .on_ended = sc_report_on_recorder_ended,
    };

    sc_mutex_lock(&report->mutex);
    report->recorder_failed = false;
    sc_mutex_unlock(&report->mutex);

    if (!sc_recorder_init(&report->recorder, report->video_path,
                          report->video_format, video, false, orientation,
                          &cbs, report)) {
        sc_mutex_lock(&report->mutex);
        report->recorder_failed = true;
        sc_mutex_unlock(&report->mutex);
        return false;
    }
    report->recorder_initialized = true;

    if (video) {
        // Enable this before the worker starts. Encoded preroll may be queued,
        // but it must not be consumed until the exact PTS of the first
        // successfully retained decoded frame has been published.
        sc_recorder_require_video_pts_origin(&report->recorder);
    }

    if (!sc_recorder_start(&report->recorder)) {
        sc_recorder_destroy(&report->recorder);
        report->recorder_initialized = false;
        sc_mutex_lock(&report->mutex);
        report->recorder_failed = true;
        sc_mutex_unlock(&report->mutex);
        return false;
    }
    report->recorder_started = true;
    return true;
}

struct sc_packet_sink *
sc_report_video_sink(struct sc_report *report) {
    return &report->video_packet_sink;
}

bool
sc_report_failed(struct sc_report *report) {
    sc_mutex_lock(&report->mutex);
    bool failed = report->recorder_failed || report->io_failed;
    sc_mutex_unlock(&report->mutex);
    return failed;
}

bool
sc_report_get_timeline_time_ms(struct sc_report *report, int64_t *out_ms) {
    assert(out_ms);
    return report->keeper
        && sc_frame_keeper_video_time_ms(report->keeper, out_ms);
}

void
sc_report_stop_recording(struct sc_report *report) {
    // Serialize finalization with event writes. Once this gate closes, no
    // request may append after ended_at or make manifest.event_count stale.
    sc_mutex_lock(&report->mutex);
    report->accepting_events = false;
    uint64_t event_count = report->seq;
    FILE *events = report->events;
    report->events = NULL;
    sc_mutex_unlock(&report->mutex);

    if (events) {
        bool flushed = fflush(events) == 0;
        bool closed = fclose(events) == 0;
        if (!flushed || !closed) {
            sc_mutex_lock(&report->mutex);
            report->io_failed = true;
            sc_mutex_unlock(&report->mutex);
            LOGE("Test report: could not finalize event log");
        }
    }

    struct sc_size video_size = {0, 0};
    if (report->keeper) {
        // wait_size() also provides the synchronized size snapshot. If a
        // frame already exists then it returns immediately; otherwise the
        // current deadline makes this a non-blocking read.
        sc_frame_keeper_wait_size(report->keeper, sc_tick_now(), &video_size);
    }

    int64_t duration_ms = 0;
    bool has_anchor =
        sc_report_get_timeline_time_ms(report, &duration_ms);
    if (report->recorder_started && !sc_report_publish_video_origin(report)) {
        has_anchor = false;
    }
    if (!has_anchor) {
        LOGE("Test report: stopped before the first decoded-frame timeline "
             "anchor was established");
        sc_report_mark_failed(report);
    }

    if (report->recorder_started) {
        // The event gate is closed and the demuxer/frame keeper have stopped
        // before normal session finalization reaches here. Publish the one
        // authoritative frozen tail target before stopping the recorder so the
        // final held frame covers every accepted event without moving any event
        // coordinate.
        sc_recorder_set_video_end(&report->recorder, duration_ms * 1000);
        sc_recorder_stop(&report->recorder);
        sc_recorder_join(&report->recorder);
        report->recorder_started = false;

        int64_t actual_us =
            sc_recorder_get_video_duration(&report->recorder);
        if (actual_us > 0) {
            duration_ms = (actual_us + 999) / 1000;
        }
    }
    if (report->recorder_initialized) {
        sc_recorder_destroy(&report->recorder);
        report->recorder_initialized = false;
    }

    char ended[32];
    iso8601_now(ended, sizeof(ended));
    bool failed = sc_report_failed(report);
    if (!write_manifest(report, !failed, video_size.width, video_size.height,
                        duration_ms, ended, event_count)) {
        sc_mutex_lock(&report->mutex);
        report->io_failed = true;
        sc_mutex_unlock(&report->mutex);
        failed = true;
    }

    LOGI("Test report: %s (%" PRIu64 " events, %" PRId64 " ms)",
         failed ? "recording failed" : "finalized", event_count, duration_ms);
}

static bool
sc_report_write_event_at(struct sc_report *report, int64_t t_ms,
                         const char *op, const char *action,
                         const char *extra_json) {
    if (t_ms < 0) {
        LOGE("Test report: refusing to log event with a negative timeline "
             "timestamp");
        sc_report_mark_failed(report);
        return false;
    }

    char wall[32];
    iso8601_now(wall, sizeof(wall));

    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        sc_report_mark_failed(report);
        return false;
    }

    sc_mutex_lock(&report->mutex);
    if (!report->accepting_events) {
        sc_mutex_unlock(&report->mutex);
        free(buf.s);
        return false;
    }
    uint64_t seq = report->seq;

    char head[128];
    snprintf(head, sizeof(head),
             "{\"seq\":%" PRIu64 ",\"t_ms\":%" PRId64 ",\"wall\":\"%s\",\"op\":",
             seq, t_ms, wall);

    bool w = sc_strbuf_append_str(&buf, head)
          && sc_json_append_escaped(&buf, op)
          && sc_strbuf_append_staticstr(&buf, ",\"action\":");
    if (action) {
        w = w && sc_json_append_escaped(&buf, action);
    } else {
        w = w && sc_strbuf_append_staticstr(&buf, "null");
    }
    if (extra_json && *extra_json) {
        w = w && sc_strbuf_append_char(&buf, ',')
             && sc_strbuf_append_str(&buf, extra_json);
    }
    w = w && sc_strbuf_append_staticstr(&buf, "}\n");

    bool wrote = w
              && fwrite(buf.s, 1, buf.len, report->events) == buf.len
              && fflush(report->events) == 0;
    if (wrote) {
        ++report->seq;
    } else {
        report->io_failed = true;
        report->accepting_events = false;
        LOGE("Test report: could not persist event log");
    }

    sc_mutex_unlock(&report->mutex);
    free(buf.s);
    return wrote;
}

bool
sc_report_log_event_at(struct sc_report *report, int64_t t_ms,
                       const char *op, const char *action,
                       const char *extra_json) {
    int64_t current_ms;
    if (!sc_report_get_timeline_time_ms(report, &current_ms)) {
        LOGE("Test report: refusing to log an event before the first decoded "
             "frame established the timeline anchor");
        sc_report_mark_failed(report);
        return false;
    }
    if (t_ms > current_ms) {
        LOGE("Test report: refusing to log an event in the future "
             "(event=%" PRId64 "ms, current=%" PRId64 "ms)",
             t_ms, current_ms);
        sc_report_mark_failed(report);
        return false;
    }

    return sc_report_write_event_at(report, t_ms, op, action, extra_json);
}

bool
sc_report_log_event(struct sc_report *report, const char *op,
                    const char *action, const char *extra_json) {
    int64_t t_ms;
    if (!sc_report_get_timeline_time_ms(report, &t_ms)) {
        LOGE("Test report: refusing to log an event before the first decoded "
             "frame established the timeline anchor");
        sc_report_mark_failed(report);
        return false;
    }

    return sc_report_write_event_at(report, t_ms, op, action, extra_json);
}

void
sc_report_destroy(struct sc_report *report) {
    if (!report->dir) {
        return; // never successfully initialized
    }
    if (report->events) {
        fclose(report->events);
        report->events = NULL;
    }
    sc_mutex_destroy(&report->mutex);
    free(report->video_path);
    free(report->dir);
    report->dir = NULL;
}
