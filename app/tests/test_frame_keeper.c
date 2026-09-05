#include "common.h"

#include <assert.h>

#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>

#include "daemon/frame_keeper.h"

static AVFrame *
make_frame(int64_t pts, int width, int height) {
    AVFrame *frame = av_frame_alloc();
    assert(frame);
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;
    frame->pts = pts;
    assert(av_frame_get_buffer(frame, 32) == 0);
    return frame;
}

static void
test_first_anchor_is_immutable_and_resettable(void) {
    struct sc_frame_keeper keeper;
    assert(sc_frame_keeper_init(&keeper));

    struct sc_frame_sink *sink = &keeper.frame_sink;
    assert(sink->ops->open(sink, NULL, NULL));

    sc_tick tick;
    int64_t pts;
    assert(!sc_frame_keeper_get_timeline_anchor(&keeper, &tick, &pts));

    AVFrame *first = make_frame(123456, 16, 32);
    assert(sink->ops->push(sink, first));

    sc_tick first_tick;
    int64_t first_pts;
    assert(sc_frame_keeper_get_timeline_anchor(&keeper, &first_tick,
                                               &first_pts));
    assert(first_tick != 0);
    assert(first_pts == 123456);

    struct sc_size size;
    assert(sc_frame_keeper_wait_size(&keeper, sc_tick_now(), &size));
    assert(size.width == 16);
    assert(size.height == 32);

    AVFrame *second = make_frame(999999, 64, 48);
    assert(sink->ops->push(sink, second));

    sc_tick unchanged_tick;
    int64_t unchanged_pts;
    assert(sc_frame_keeper_get_timeline_anchor(&keeper, &unchanged_tick,
                                               &unchanged_pts));
    assert(unchanged_tick == first_tick);
    assert(unchanged_pts == first_pts);

    sink->ops->close(sink);
    int64_t closed_time_ms;
    assert(sc_frame_keeper_video_time_ms(&keeper, &closed_time_ms));
    assert(closed_time_ms >= 0);

    sc_frame_keeper_reset(&keeper);
    assert(!sc_frame_keeper_get_timeline_anchor(&keeper, &tick, &pts));
    assert(!sc_frame_keeper_video_time_ms(&keeper, &closed_time_ms));
    assert(sc_frame_keeper_last_tick(&keeper) == 0);

    assert(sink->ops->open(sink, NULL, NULL));
    AVFrame *third = make_frame(777000, 20, 40);
    assert(sink->ops->push(sink, third));
    assert(sc_frame_keeper_get_timeline_anchor(&keeper, &tick, &pts));
    assert(pts == 777000);
    sink->ops->close(sink);

    av_frame_free(&third);
    av_frame_free(&second);
    av_frame_free(&first);
    sc_frame_keeper_destroy(&keeper);
}

static void
test_first_frame_without_pts_does_not_create_anchor(void) {
    struct sc_frame_keeper keeper;
    assert(sc_frame_keeper_init(&keeper));

    struct sc_frame_sink *sink = &keeper.frame_sink;
    assert(sink->ops->open(sink, NULL, NULL));

    AVFrame *frame = make_frame(AV_NOPTS_VALUE, 16, 16);
    assert(!sink->ops->push(sink, frame));

    sc_tick tick;
    int64_t pts;
    assert(!sc_frame_keeper_get_timeline_anchor(&keeper, &tick, &pts));
    assert(sc_frame_keeper_last_tick(&keeper) == 0);

    int64_t time_ms;
    assert(!sc_frame_keeper_video_time_ms(&keeper, &time_ms));

    sink->ops->close(sink);
    av_frame_free(&frame);
    sc_frame_keeper_destroy(&keeper);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_first_anchor_is_immutable_and_resettable();
    test_first_frame_without_pts_does_not_create_anchor();
    return 0;
}
