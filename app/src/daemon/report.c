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

#include "daemon/protocol.h" // sc_json_append_escaped
#include "util/log.h"
#include "util/strbuf.h"

static bool
make_dir(const char *path) {
#ifdef _WIN32
    int r = _mkdir(path);
#else
    int r = mkdir(path, 0755);
#endif
    return r == 0 || errno == EEXIST;
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
    struct tm *tm = gmtime(&now);
    if (!tm || strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", tm) == 0) {
        out[0] = '\0';
    }
}

static void
sc_report_on_recorder_ended(struct sc_recorder *recorder, bool success,
                            void *userdata) {
    (void) recorder;
    struct sc_report *report = userdata;
    if (!success) {
        sc_mutex_lock(&report->mutex);
        report->recorder_failed = true;
        sc_mutex_unlock(&report->mutex);
        LOGE("Test report: recording failed");
    }
}

// Write manifest.json. `finalized` marks a clean end.
static void
write_manifest(struct sc_report *report, bool finalized, int width, int height,
               int64_t duration_ms, const char *ended_at) {
    char *path = join(report->dir, "manifest.json");
    if (!path) {
        return;
    }

    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 512)) {
        free(path);
        return;
    }

    char head[256];
    snprintf(head, sizeof(head),
             "{\"report_version\":1,\"app\":\"scrcpy-auto\",");
    bool w = sc_strbuf_append_str(&buf, head)
          && sc_strbuf_append_staticstr(&buf, "\"serial\":")
          && sc_json_append_escaped(&buf, report->serial)
          && sc_strbuf_append_staticstr(&buf, ",\"device_name\":")
          && sc_json_append_escaped(&buf, report->device_name);

    char tail[384];
    snprintf(tail, sizeof(tail),
             ",\"video\":{\"file\":\"recording.mp4\",\"codec\":\"h264\","
             "\"width\":%d,\"height\":%d,\"duration_ms\":%" PRId64 ","
             "\"finalized\":%s},\"ended_at\":%s%s%s,\"event_count\":%"
             PRIu64 "}\n",
             width, height, duration_ms, finalized ? "true" : "false",
             ended_at ? "\"" : "null", ended_at ? ended_at : "",
             ended_at ? "\"" : "", report->seq);
    w = w && sc_strbuf_append_str(&buf, tail);

    if (w) {
        FILE *fp = fopen(path, "w");
        if (fp) {
            fwrite(buf.s, 1, buf.len, fp);
            fclose(fp);
        } else {
            LOGW("Test report: could not write manifest: %s", path);
        }
    }

    free(buf.s);
    free(path);
}

bool
sc_report_init(struct sc_report *report, const char *dir,
               struct sc_frame_keeper *keeper, const char *serial,
               const char *device_name) {
    report->dir = strdup(dir);
    if (!report->dir) {
        LOG_OOM();
        return false;
    }

    if (!make_dir(report->dir)) {
        LOGE("Test report: could not create directory: %s", report->dir);
        goto error_free_dir;
    }

    report->video_path = join(report->dir, "recording.mp4");
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
    report->recorder_initialized = false;
    report->recorder_started = false;
    report->recorder_failed = false;
    report->seq = 0;
    snprintf(report->serial, sizeof(report->serial), "%s",
             serial ? serial : "");
    snprintf(report->device_name, sizeof(report->device_name), "%s",
             device_name ? device_name : "");

    write_manifest(report, false, 0, 0, 0, NULL);

    LOGI("Test report: writing to %s", report->dir);
    return true;

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

    if (!sc_recorder_init(&report->recorder, report->video_path,
                          SC_RECORD_FORMAT_MP4, video, false, orientation,
                          &cbs, report)) {
        return false;
    }
    report->recorder_initialized = true;

    if (!sc_recorder_start(&report->recorder)) {
        sc_recorder_destroy(&report->recorder);
        report->recorder_initialized = false;
        return false;
    }
    report->recorder_started = true;
    return true;
}

struct sc_packet_sink *
sc_report_video_sink(struct sc_report *report) {
    return &report->recorder.video_packet_sink;
}

void
sc_report_stop_recording(struct sc_report *report) {
    int width = report->keeper ? report->keeper->size.width : 0;
    int height = report->keeper ? report->keeper->size.height : 0;

    int64_t duration_ms = 0;
    if (report->keeper) {
        sc_frame_keeper_video_time_ms(report->keeper, &duration_ms);
    }

    if (report->recorder_started) {
        sc_recorder_stop(&report->recorder);
        sc_recorder_join(&report->recorder);
        report->recorder_started = false;
    }
    if (report->recorder_initialized) {
        sc_recorder_destroy(&report->recorder);
        report->recorder_initialized = false;
    }

    char ended[32];
    iso8601_now(ended, sizeof(ended));
    write_manifest(report, true, width, height, duration_ms, ended);

    LOGI("Test report: finalized (%" PRIu64 " events, %" PRId64 " ms)",
         report->seq, duration_ms);
}

void
sc_report_log_event(struct sc_report *report, const char *op,
                    const char *action, const char *extra_json) {
    int64_t t_ms = 0;
    if (report->keeper) {
        sc_frame_keeper_video_time_ms(report->keeper, &t_ms);
    }

    char wall[32];
    iso8601_now(wall, sizeof(wall));

    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        return;
    }

    sc_mutex_lock(&report->mutex);
    uint64_t seq = report->seq++;
    sc_mutex_unlock(&report->mutex);

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

    if (w) {
        sc_mutex_lock(&report->mutex);
        fwrite(buf.s, 1, buf.len, report->events);
        fflush(report->events);
        sc_mutex_unlock(&report->mutex);
    }

    free(buf.s);
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
