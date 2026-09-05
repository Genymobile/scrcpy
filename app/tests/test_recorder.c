#include "common.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/pixfmt.h>

#include "compat.h"
#include "recorder.h"

struct recorder_result {
    bool called;
    bool success;
};

static void
on_ended(struct sc_recorder *recorder, bool success, void *userdata) {
    (void) recorder;
    struct recorder_result *result = userdata;
    result->called = true;
    result->success = success;
}

static void
push_data_packet(struct sc_packet_sink *sink, int64_t pts, bool key,
                 const uint8_t *payload, size_t payload_size) {
    AVPacket *packet = av_packet_alloc();
    assert(packet);
    assert(payload_size <= INT_MAX);
    assert(av_new_packet(packet, (int) payload_size) == 0);
    memcpy(packet->data, payload, payload_size);
    packet->pts = pts;
    packet->dts = pts;
    if (key) {
        packet->flags |= AV_PKT_FLAG_KEY;
    }
    assert(sink->ops->push(sink, packet));
    av_packet_free(&packet);
}

static void
push_packet(struct sc_packet_sink *sink, int64_t pts, bool key) {
    static const uint8_t payload[] = {0x10, 0x00, 0x00, 0x00};
    push_data_packet(sink, pts, key, payload, sizeof(payload));
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

static void
push_config(struct sc_packet_sink *sink, enum AVCodecID codec_id) {
    if (codec_id == AV_CODEC_ID_H264) {
        // Baseline-profile Annex-B SPS/PPS for a tiny synthetic stream.
        static const uint8_t h264_config[] = {
            0x00, 0x00, 0x00, 0x01,
            0x67, 0x42, 0xc0, 0x0a, 0xda, 0x7b, 0x01, 0x10,
            0x00, 0x00, 0x03, 0x00, 0x10, 0x00, 0x00, 0x03,
            0x03, 0x20, 0xf1, 0x22, 0x6a,
            0x00, 0x00, 0x00, 0x01,
            0x68, 0xce, 0x0f, 0xc8,
        };
        push_data_packet(sink, AV_NOPTS_VALUE, false, h264_config,
                         sizeof(h264_config));
        return;
    }

    if (codec_id == AV_CODEC_ID_HEVC) {
        // Annex-B VPS/SPS/PPS for a tiny synthetic HEVC stream.
        static const uint8_t hevc_config[] = {
            0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01,
            0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
            0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
            0x1e, 0x95, 0x94, 0x09,
            0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
            0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00,
            0x03, 0x00, 0x00, 0x03, 0x00, 0x1e, 0xa0, 0x20,
            0x81, 0x05, 0x96, 0x56, 0x54, 0xa4, 0xc2, 0xf0,
            0x16, 0x80, 0x80, 0x00, 0x00, 0x03, 0x00, 0x80,
            0x00, 0x00, 0x03, 0x00, 0x84,
            0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc0, 0x73,
            0xc0, 0x89,
        };
        push_data_packet(sink, AV_NOPTS_VALUE, false, hevc_config,
                         sizeof(hevc_config));
        return;
    }

#ifdef SCRCPY_LAVC_HAS_AV1
    if (codec_id == AV_CODEC_ID_AV1) {
        // Sequence-header OBU for a tiny 8-bit 4:2:0 stream. The recorder
        // stores the initial copy as extradata and prepends a late copy to the
        // next media sample.
        static const uint8_t av1_config[] = {
            0x0a, 0x0a, 0x02, 0x00, 0x00, 0x05,
            0x0c, 0xff, 0xc4, 0xaf, 0x90, 0x04,
        };
        push_data_packet(sink, AV_NOPTS_VALUE, false, av1_config,
                         sizeof(av1_config));
        return;
    }
#endif

    push_packet(sink, AV_NOPTS_VALUE, false);
}

static void
push_media(struct sc_packet_sink *sink, enum AVCodecID codec_id, int64_t pts,
           bool key) {
#ifdef SCRCPY_LAVC_HAS_AV1
    if (codec_id == AV_CODEC_ID_AV1) {
        // Frame OBU matching the sequence header in push_config().
        static const uint8_t av1_frame[] = {
            0x32, 0x0f, 0x10, 0x00, 0x96, 0x80, 0x10, 0x40,
            0x82, 0x00, 0x00, 0x00, 0x00, 0xee, 0x6e, 0xb9,
            0xbc,
        };
        push_data_packet(sink, pts, key, av1_frame, sizeof(av1_frame));
        return;
    }
#endif

    if (codec_id == AV_CODEC_ID_HEVC) {
        static const uint8_t hevc_frame[] = {
            0x00, 0x00, 0x00, 0x0d, 0x28, 0x01, 0xac, 0x4e,
            0xd7, 0x1f, 0xff, 0xf5, 0xde, 0x9c, 0xaf, 0xea,
            0xf8,
        };
        push_data_packet(sink, pts, key, hevc_frame, sizeof(hevc_frame));
        return;
    }

    push_packet(sink, pts, key);
}

static void
run_explicit_origin_success(enum AVCodecID codec_id,
                            enum sc_record_format format,
                            const char *filename, bool initial_config,
                            bool late_config) {
    remove(filename);

    struct recorder_result result = {0};
    static const struct sc_recorder_callbacks cbs = {
        .on_ended = on_ended,
    };
    struct sc_recorder recorder;
    assert(sc_recorder_init(&recorder, filename, format, true, false,
                            SC_ORIENTATION_0, &cbs, &result));
    sc_recorder_require_video_pts_origin(&recorder);
    assert(sc_recorder_start(&recorder));

    AVCodecContext *ctx = make_video_context(codec_id);
    struct sc_packet_sink *sink = &recorder.video_packet_sink;
    assert(sink->ops->open(sink, ctx, NULL));

    if (initial_config) {
        push_config(sink, codec_id);
    }

    // Queue preroll before publishing the decoded-frame origin. The recorder
    // worker must wait, discard the leading packet, then start exactly at the
    // matching packet.
    push_media(sink, codec_id, 5000000, true);
    if (late_config) {
        push_config(sink, codec_id);
    }
    push_media(sink, codec_id, 6000000, true);
    push_media(sink, codec_id, 7000000, false);
    sc_recorder_set_video_pts_origin(&recorder, 6000000);

    sc_recorder_set_video_end(&recorder, 3000000);
    sink->ops->close(sink);
    sc_recorder_join(&recorder);

    assert(result.called);
    assert(result.success);
    assert(sc_recorder_get_video_duration(&recorder) == 3000000);

    sc_recorder_destroy(&recorder);
    avcodec_free_context(&ctx);
    remove(filename);
}

static void
test_explicit_origin_filters_preroll_for_vpx(void) {
    run_explicit_origin_success(AV_CODEC_ID_VP8, SC_RECORD_FORMAT_WEBM,
                                "test-recorder-origin-vp8.webm", false, false);
    run_explicit_origin_success(AV_CODEC_ID_VP9, SC_RECORD_FORMAT_MP4,
                                "test-recorder-origin-vp9.mp4", false, false);
}

static void
test_explicit_origin_preserves_config_paths(void) {
    // Matroska accepts the minimal synthetic payload used by this unit test;
    // the recorder's H.26x/AV1 config handling is container-independent.
    run_explicit_origin_success(AV_CODEC_ID_H264, SC_RECORD_FORMAT_MKV,
                                "test-recorder-origin-h264.mkv", true, false);
    run_explicit_origin_success(AV_CODEC_ID_HEVC, SC_RECORD_FORMAT_MKV,
                                "test-recorder-origin-h265.mkv", true, false);
#ifdef SCRCPY_LAVC_HAS_AV1
    run_explicit_origin_success(AV_CODEC_ID_AV1, SC_RECORD_FORMAT_MKV,
                                "test-recorder-origin-av1.mkv", true, true);
#endif
}

static void
test_explicit_origin_is_required(void) {
    static const char filename[] = "test-recorder-origin-missing.webm";
    remove(filename);

    struct recorder_result result = {0};
    static const struct sc_recorder_callbacks cbs = {
        .on_ended = on_ended,
    };
    struct sc_recorder recorder;
    assert(sc_recorder_init(&recorder, filename, SC_RECORD_FORMAT_WEBM,
                            true, false, SC_ORIENTATION_0, &cbs, &result));
    sc_recorder_require_video_pts_origin(&recorder);
    assert(sc_recorder_start(&recorder));

    AVCodecContext *ctx = make_video_context(AV_CODEC_ID_VP8);
    struct sc_packet_sink *sink = &recorder.video_packet_sink;
    assert(sink->ops->open(sink, ctx, NULL));
    push_packet(sink, 5000000, true);
    sink->ops->close(sink);
    sc_recorder_join(&recorder);

    assert(result.called);
    assert(!result.success);

    sc_recorder_destroy(&recorder);
    avcodec_free_context(&ctx);
    remove(filename);
}

static void
test_explicit_origin_must_match_a_packet(void) {
    static const char filename[] = "test-recorder-origin-mismatch.mp4";
    remove(filename);

    struct recorder_result result = {0};
    static const struct sc_recorder_callbacks cbs = {
        .on_ended = on_ended,
    };
    struct sc_recorder recorder;
    assert(sc_recorder_init(&recorder, filename, SC_RECORD_FORMAT_MP4,
                            true, false, SC_ORIENTATION_0, &cbs, &result));
    sc_recorder_require_video_pts_origin(&recorder);
    assert(sc_recorder_start(&recorder));

    AVCodecContext *ctx = make_video_context(AV_CODEC_ID_VP9);
    struct sc_packet_sink *sink = &recorder.video_packet_sink;
    assert(sink->ops->open(sink, ctx, NULL));
    push_packet(sink, 5000000, true);
    push_packet(sink, 6000000, true);
    sc_recorder_set_video_pts_origin(&recorder, 5500000);
    sink->ops->close(sink);
    sc_recorder_join(&recorder);

    assert(result.called);
    assert(!result.success);

    sc_recorder_destroy(&recorder);
    avcodec_free_context(&ctx);
    remove(filename);
}

static void
test_explicit_origin_must_be_a_keyframe(void) {
    static const char filename[] = "test-recorder-origin-non-key.webm";
    remove(filename);

    struct recorder_result result = {0};
    static const struct sc_recorder_callbacks cbs = {
        .on_ended = on_ended,
    };
    struct sc_recorder recorder;
    assert(sc_recorder_init(&recorder, filename, SC_RECORD_FORMAT_WEBM,
                            true, false, SC_ORIENTATION_0, &cbs, &result));
    sc_recorder_require_video_pts_origin(&recorder);
    assert(sc_recorder_start(&recorder));

    AVCodecContext *ctx = make_video_context(AV_CODEC_ID_VP8);
    struct sc_packet_sink *sink = &recorder.video_packet_sink;
    assert(sink->ops->open(sink, ctx, NULL));
    push_packet(sink, 5000000, true);
    push_packet(sink, 6000000, false);
    sc_recorder_set_video_pts_origin(&recorder, 6000000);
    sink->ops->close(sink);
    sc_recorder_join(&recorder);

    assert(result.called);
    assert(!result.success);

    sc_recorder_destroy(&recorder);
    avcodec_free_context(&ctx);
    remove(filename);
}

static void
test_exact_report_duration_stays_in_microseconds(void) {
    static const char filename[] = "test-recorder-duration.webm";
    remove(filename);

    struct recorder_result result = {0};
    static const struct sc_recorder_callbacks cbs = {
        .on_ended = on_ended,
    };
    struct sc_recorder recorder;
    assert(sc_recorder_init(&recorder, filename, SC_RECORD_FORMAT_WEBM,
                            true, false, SC_ORIENTATION_0, &cbs, &result));
    assert(sc_recorder_start(&recorder));

    AVCodecContext *ctx = make_video_context(AV_CODEC_ID_VP8);

    struct sc_packet_sink *sink = &recorder.video_packet_sink;
    assert(sink->ops->open(sink, ctx, NULL));
    push_packet(sink, 5000000, true);
    push_packet(sink, 6000000, false);

    // Relative to the first packet: hold the second frame through exactly 3s.
    sc_recorder_set_video_end(&recorder, 3000000);
    sink->ops->close(sink);
    sc_recorder_join(&recorder);

    assert(result.called);
    assert(result.success);
    // The muxer rescales packets (WebM uses milliseconds). This getter must
    // nevertheless stay in its documented microsecond domain.
    assert(sc_recorder_get_video_duration(&recorder) == 3000000);

    sc_recorder_destroy(&recorder);
    avcodec_free_context(&ctx);
    remove(filename);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_exact_report_duration_stays_in_microseconds();
    test_explicit_origin_filters_preroll_for_vpx();
    test_explicit_origin_preserves_config_paths();
    test_explicit_origin_is_required();
    test_explicit_origin_must_match_a_packet();
    test_explicit_origin_must_be_a_keyframe();
    return 0;
}
