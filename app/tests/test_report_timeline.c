#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>

#include "daemon/frame_keeper.h"
#include "daemon/report.h"
#include "util/tick.h"

static AVFrame *
make_frame(int64_t pts) {
    AVFrame *frame = av_frame_alloc();
    assert(frame);
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = 16;
    frame->height = 16;
    frame->pts = pts;
    assert(av_frame_get_buffer(frame, 32) == 0);
    return frame;
}

static size_t
read_report_file(const char *dir, const char *name, char *buf, size_t size) {
    char path[256];
    int r = snprintf(path, sizeof(path), "%s/%s", dir, name);
    assert(r > 0 && (size_t) r < sizeof(path));

    FILE *fp = fopen(path, "rb");
    assert(fp);
    size_t count = fread(buf, 1, size - 1, fp);
    assert(!ferror(fp));
    assert(feof(fp));
    buf[count] = '\0';
    assert(fclose(fp) == 0);
    return count;
}

static size_t
read_events(const char *dir, char *buf, size_t size) {
    return read_report_file(dir, "events.jsonl", buf, size);
}

static void
cleanup_report(const char *dir) {
    static const char *const files[] = {
        "events.jsonl",
        "manifest.json",
        "manifest.json.tmp",
        "recording.mp4",
        "recording.webm",
    };

    char path[256];
    for (size_t i = 0; i < ARRAY_LEN(files); ++i) {
        int r = snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        assert(r > 0 && (size_t) r < sizeof(path));
        (void) remove(path);
    }
    (void) remove(dir);
}

static void
make_report_dir(char *dir, size_t size, const char *suffix) {
    int r = snprintf(dir, size, "test-report-timeline-%" PRIu64 "-%s",
                     (uint64_t) sc_tick_now(), suffix);
    assert(r > 0 && (size_t) r < size);
}

static AVCodecContext *
make_video_context(enum AVCodecID codec_id) {
    AVCodecContext *ctx = avcodec_alloc_context3(NULL);
    assert(ctx);
    ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ctx->codec_id = codec_id;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->width = 16;
    ctx->height = 16;
    return ctx;
}

static AVPacket *
make_packet(int64_t pts) {
    AVPacket *packet = av_packet_alloc();
    assert(packet);
    assert(av_new_packet(packet, 4) == 0);
    static const uint8_t payload[] = {0x10, 0x00, 0x00, 0x00};
    memcpy(packet->data, payload, sizeof(payload));
    packet->pts = pts;
    packet->dts = pts;
    packet->flags |= AV_PKT_FLAG_KEY;
    return packet;
}

static int64_t
read_recording_duration_us(const char *dir, const char *name) {
    char path[256];
    int r = snprintf(path, sizeof(path), "%s/%s", dir, name);
    assert(r > 0 && (size_t) r < sizeof(path));

    AVFormatContext *ctx = NULL;
    assert(avformat_open_input(&ctx, path, NULL, NULL) == 0);
    assert(avformat_find_stream_info(ctx, NULL) >= 0);
    int64_t duration_us = ctx->duration;
    avformat_close_input(&ctx);
    return duration_us;
}

static void
test_pre_anchor_event_is_not_persisted_as_zero(void) {
    struct sc_frame_keeper keeper;
    assert(sc_frame_keeper_init(&keeper));

    char dir[128];
    make_report_dir(dir, sizeof(dir), "pre-anchor");

    struct sc_report report;
    assert(sc_report_init(&report, dir, &keeper, NULL, "serial", "device",
                          SC_CODEC_H264));

    assert(!sc_report_log_event(&report, "control", "too early", NULL));
    assert(sc_report_failed(&report));
    sc_report_destroy(&report);

    char events[256];
    assert(read_events(dir, events, sizeof(events)) == 0);

    cleanup_report(dir);
    sc_frame_keeper_destroy(&keeper);
}

static void
test_zero_is_valid_after_anchor_and_future_is_rejected(void) {
    struct sc_frame_keeper keeper;
    assert(sc_frame_keeper_init(&keeper));

    struct sc_frame_sink *sink = &keeper.frame_sink;
    assert(sink->ops->open(sink, NULL, NULL));

    char dir[128];
    make_report_dir(dir, sizeof(dir), "anchored");

    struct sc_report report;
    assert(sc_report_init(&report, dir, &keeper, NULL, "serial", "device",
                          SC_CODEC_H264));

    AVFrame *first = make_frame(4242000);
    assert(sink->ops->push(sink, first));

    assert(sc_report_log_event_at(&report, 0, "plugin", "first frame",
                                  "\"status\":\"ok\""));

    int64_t current_ms;
    assert(sc_report_get_timeline_time_ms(&report, &current_ms));
    assert(!sc_report_log_event_at(&report, current_ms + 60000, "plugin",
                                   "future", NULL));
    assert(report.seq == 1);

    sc_report_destroy(&report);
    sink->ops->close(sink);
    av_frame_free(&first);

    char events[1024];
    size_t count = read_events(dir, events, sizeof(events));
    assert(count > 0);
    assert(strstr(events, "\"seq\":0,\"t_ms\":0,"));
    assert(strstr(events, "\"op\":\"plugin\""));
    assert(strstr(events, "\"status\":\"ok\""));
    assert(!strstr(events, "\"action\":\"future\""));
    assert(strchr(events, '\n') == strrchr(events, '\n'));

    cleanup_report(dir);
    sc_frame_keeper_destroy(&keeper);
}

static void
test_wrapper_close_defers_final_tail_until_keeper_freezes(void) {
    struct sc_frame_keeper keeper;
    assert(sc_frame_keeper_init(&keeper));
    struct sc_frame_sink *frame_sink = &keeper.frame_sink;
    assert(frame_sink->ops->open(frame_sink, NULL, NULL));

    char dir[128];
    make_report_dir(dir, sizeof(dir), "deferred-tail");

    struct sc_report report;
    assert(sc_report_init(&report, dir, &keeper, NULL, "serial", "device",
                          SC_CODEC_VP8));
    assert(sc_report_start_recording(&report, true, SC_ORIENTATION_0));

    struct sc_packet_sink *video_sink = sc_report_video_sink(&report);
    AVCodecContext *ctx = make_video_context(AV_CODEC_ID_VP8);
    assert(video_sink->ops->open(video_sink, ctx, NULL));

    static const int64_t origin = 6000000;
    AVFrame *first = make_frame(origin);
    assert(frame_sink->ops->push(frame_sink, first));
    AVPacket *packet = make_packet(origin);
    assert(video_sink->ops->push(video_sink, packet));

    int64_t early_ms;
    assert(sc_frame_keeper_video_time_ms(&keeper, &early_ms));

    // This is the real demuxer close order: the report wrapper closes before
    // the decoder closes (and therefore freezes) the frame keeper.
    video_sink->ops->close(video_sink);
    sc_mutex_lock(&report.recorder.mutex);
    bool stopped_early = report.recorder.stopped;
    sc_mutex_unlock(&report.recorder.mutex);
    assert(!stopped_early);

    // Make the later keeper close observably different without depending on a
    // scheduler sleep primitive. The production clock has microsecond
    // precision; a short bounded busy wait is sufficient for this unit test.
    int64_t running_ms;
    do {
        assert(sc_frame_keeper_video_time_ms(&keeper, &running_ms));
    } while (running_ms < early_ms + 5);

    frame_sink->ops->close(frame_sink);
    int64_t final_ms;
    assert(sc_frame_keeper_video_time_ms(&keeper, &final_ms));
    assert(final_ms >= early_ms + 5);

    sc_report_stop_recording(&report);
    assert(!sc_report_failed(&report));
    sc_report_destroy(&report);

    char manifest[2048];
    assert(read_report_file(dir, "manifest.json", manifest,
                            sizeof(manifest)) > 0);
    char duration[64];
    int r = snprintf(duration, sizeof(duration),
                     "\"duration_ms\":%" PRId64, final_ms);
    assert(r > 0 && (size_t) r < sizeof(duration));
    assert(strstr(manifest, duration));
    assert(strstr(manifest, "\"finalized\":true"));
    assert(read_recording_duration_us(dir, "recording.webm")
            == final_ms * 1000);

    av_packet_free(&packet);
    av_frame_free(&first);
    avcodec_free_context(&ctx);
    cleanup_report(dir);
    sc_frame_keeper_destroy(&keeper);
}

static void
test_startup_failure_without_first_frame_still_joins_recorder(void) {
    struct sc_frame_keeper keeper;
    assert(sc_frame_keeper_init(&keeper));
    struct sc_frame_sink *frame_sink = &keeper.frame_sink;
    assert(frame_sink->ops->open(frame_sink, NULL, NULL));

    char dir[128];
    make_report_dir(dir, sizeof(dir), "no-first-frame");

    struct sc_report report;
    assert(sc_report_init(&report, dir, &keeper, NULL, "serial", "device",
                          SC_CODEC_VP8));
    assert(sc_report_start_recording(&report, true, SC_ORIENTATION_0));

    struct sc_packet_sink *video_sink = sc_report_video_sink(&report);
    AVCodecContext *ctx = make_video_context(AV_CODEC_ID_VP8);
    assert(video_sink->ops->open(video_sink, ctx, NULL));

    // Startup may fail after the report recorder thread and its packet sink
    // were opened but before a decodable frame established the anchor.
    video_sink->ops->close(video_sink);
    frame_sink->ops->close(frame_sink);
    sc_report_stop_recording(&report);
    assert(sc_report_failed(&report));
    assert(!report.recorder_started);
    sc_report_destroy(&report);

    char manifest[2048];
    assert(read_report_file(dir, "manifest.json", manifest,
                            sizeof(manifest)) > 0);
    assert(strstr(manifest, "\"finalized\":false"));

    avcodec_free_context(&ctx);
    cleanup_report(dir);
    sc_frame_keeper_destroy(&keeper);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_pre_anchor_event_is_not_persisted_as_zero();
    test_zero_is_valid_after_anchor_and_future_is_rejected();
    test_wrapper_close_defers_final_tail_until_keeper_freezes();
    test_startup_failure_without_first_frame_still_joins_recorder();
    return 0;
}
