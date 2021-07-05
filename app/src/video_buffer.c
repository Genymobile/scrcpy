#include "video_buffer.h"

#include <assert.h>
#include <stdlib.h>

#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

static void
sc_clock_history_init(struct sc_clock_history *history) {
    history->count = 0;
    history->head = 0;
    history->system_left_sum = 0;
    history->stream_left_sum = 0;
    history->system_right_sum = 0;
    history->stream_right_sum = 0;
    memset(history->points, 0, sizeof(history->points));
}

static void
sc_clock_history_push(struct sc_clock_history *history,
                      sc_tick system, sc_tick stream) {
    struct sc_clock_point *point = &history->points[history->head];
    struct sc_clock_point *mid_point = &history->points[(history->head + SC_CLOCK_RANGE/2) % SC_CLOCK_RANGE];
        history->system_left_sum += mid_point->system - point->system;
        history->stream_left_sum += mid_point->stream - point->stream;
        history->system_right_sum -= mid_point->system;
        history->stream_right_sum -= mid_point->stream;
    if (history->count == SC_CLOCK_RANGE) {
    } else {
        ++history->count;
    }

    point->system = system;
    point->stream = stream;

    history->system_right_sum += system;
    history->stream_right_sum += stream;

    history->head = (history->head + 1) % SC_CLOCK_RANGE;
}

static void
sc_clock_history_get_eq(struct sc_clock_history *history, double *coeff,
                        sc_tick *offset) {
    struct sc_clock_point p1 = {
        .system = history->system_left_sum * 2 / SC_CLOCK_RANGE,
        .stream = history->stream_left_sum * 2 / SC_CLOCK_RANGE,
    };
    struct sc_clock_point p2 = {
        .system = history->system_right_sum * 2 / SC_CLOCK_RANGE,
        .stream = history->stream_right_sum * 2 / SC_CLOCK_RANGE,
    };
    double a = (double) (p2.system - p1.system) / (p2.stream - p1.stream);
    fprintf(stderr, "%ld %ld\n", history->system_left_sum, history->system_right_sum);
    sc_tick b = (history->system_left_sum + history->system_right_sum) / SC_CLOCK_RANGE
              - (sc_tick) ((history->stream_left_sum + history->stream_right_sum) / SC_CLOCK_RANGE * a);

    *coeff = a;
    *offset = b;
}

//static void
//sc_clock_history_get_average_point(struct sc_clock_history *history,
//                                   struct sc_clock_point *point) {
//    assert(history->count);
//    point->system = history->system_sum / history->count;
//    point->stream = history->stream_sum / history->count;
//}

static void
sc_clock_init(struct sc_clock *clock) {
    clock->coeff = 1;
    clock->offset = 0;
    clock->weight = 0;

    clock->last.system = 0;
    clock->last.stream = 0;

    sc_clock_history_init(&clock->history);
}

static sc_tick
sc_clock_to_system_ts(struct sc_clock *clock, sc_tick stream_ts) {
    assert(clock->weight); // sc_clock_update() must have been called
    return (sc_tick) (stream_ts * clock->coeff) + clock->offset;
}

static void
sc_clock_update(struct sc_clock *clock, sc_tick now, sc_tick stream_ts) {
    double instant_coeff;
    if (clock->weight) {
        sc_tick system_delta = now - clock->last.system;
        sc_tick stream_delta = stream_ts - clock->last.stream;
        instant_coeff = (double) system_delta / stream_delta;
    } else {
        // This is the first update, we cannot compute delta
        instant_coeff = 1;
    }

    sc_clock_history_push(&clock->history, now, stream_ts);

    if (clock->weight < SC_CLOCK_RANGE) {
        ++clock->weight;
    }

    sc_clock_history_get_eq(&clock->history, &clock->coeff, &clock->offset);
//    // (1-t) * avg + t * new
//    clock->coeff = ((clock->weight - 1) * clock->coeff + instant_coeff)
//                 / clock->weight;
//
//    struct sc_clock_point center;
//    sc_clock_history_get_average_point(&clock->history, &center);
//
//    clock->offset = center.system - (sc_tick) (center.stream * clock->coeff);
//
    LOGD("%g x + %ld", clock->coeff, clock->offset);

    clock->last.system = now;
    clock->last.stream = stream_ts;
}

static struct sc_video_buffer_frame *
sc_video_buffer_frame_new(const AVFrame *frame) {
    struct sc_video_buffer_frame *vb_frame = malloc(sizeof(*vb_frame));
    if (!vb_frame) {
        return NULL;
    }

    vb_frame->frame = av_frame_alloc();
    if (!vb_frame->frame) {
        free(vb_frame);
        return NULL;
    }

    if (av_frame_ref(vb_frame->frame, frame)) {
        av_frame_free(&vb_frame->frame);
        free(vb_frame);
        return NULL;
    }

    return vb_frame;
}

static void
sc_video_buffer_frame_delete(struct sc_video_buffer_frame *vb_frame) {
    av_frame_unref(vb_frame->frame);
    av_frame_free(&vb_frame->frame);
    free(vb_frame);
}

static bool
sc_video_buffer_offer(struct sc_video_buffer *vb, const AVFrame *frame) {
    bool previous_skipped;
    bool ok = sc_frame_buffer_push(&vb->fb, frame, &previous_skipped);
    if (!ok) {
        return false;
    }

    vb->cbs->on_new_frame(vb, previous_skipped, vb->cbs_userdata);
    return true;
}

static int
run_buffering(void *data) {
    struct sc_video_buffer *vb = data;

    assert(vb->buffering_ms);

    for (;;) {
        sc_mutex_lock(&vb->b.mutex);

        while (!vb->b.stopped && sc_queue_is_empty(&vb->b.queue)) {
            sc_cond_wait(&vb->b.queue_cond, &vb->b.mutex);
        }

        if (vb->b.stopped) {
            sc_mutex_unlock(&vb->b.mutex);
            goto stopped;
        }

        struct sc_video_buffer_frame *vb_frame;
        sc_queue_take(&vb->b.queue, next, &vb_frame);

        sc_tick now = sc_tick_now();
        int64_t pts = vb_frame->frame->pts;
        LOGD("==== pts = %ld", pts);

        bool timed_out = false;
        while (!vb->b.stopped && !timed_out) {
            sc_tick deadline = sc_clock_to_system_ts(&vb->b.clock, pts)
                               + vb->buffering_ms / 2;
            if (deadline > now + vb->buffering_ms) {
                deadline = now + vb->buffering_ms;
            }

            timed_out =
                !sc_cond_timedwait(&vb->b.wait_cond, &vb->b.mutex, deadline);
        }

        if (vb->b.stopped) {
            sc_video_buffer_frame_delete(vb_frame);
            sc_mutex_unlock(&vb->b.mutex);
            goto stopped;
        }

        sc_mutex_unlock(&vb->b.mutex);

        sc_video_buffer_offer(vb, vb_frame->frame);

        sc_video_buffer_frame_delete(vb_frame);
    }

stopped:
    // Flush queue
    while (!sc_queue_is_empty(&vb->b.queue)) {
        struct sc_video_buffer_frame *vb_frame;
        sc_queue_take(&vb->b.queue, next, &vb_frame);
        sc_video_buffer_frame_delete(vb_frame);
    }

    LOGD("Buffering thread ended");

    return 0;
}

bool
sc_video_buffer_init(struct sc_video_buffer *vb, unsigned buffering_ms,
                     const struct sc_video_buffer_callbacks *cbs,
                     void *cbs_userdata) {
    bool ok = sc_frame_buffer_init(&vb->fb);
    if (!ok) {
        return false;
    }

    if (buffering_ms) {
        ok = sc_mutex_init(&vb->b.mutex);
        if (!ok) {
            LOGC("Could not create mutex");
            sc_frame_buffer_destroy(&vb->fb);
            return false;
        }

        ok = sc_cond_init(&vb->b.queue_cond);
        if (!ok) {
            LOGC("Could not create cond");
            sc_mutex_destroy(&vb->b.mutex);
            sc_frame_buffer_destroy(&vb->fb);
            return false;
        }

        ok = sc_cond_init(&vb->b.wait_cond);
        if (!ok) {
            LOGC("Could not create wait cond");
            sc_cond_destroy(&vb->b.queue_cond);
            sc_mutex_destroy(&vb->b.mutex);
            sc_frame_buffer_destroy(&vb->fb);
            return false;
        }

        sc_clock_init(&vb->b.clock);
        sc_queue_init(&vb->b.queue);
    }

    assert(cbs);
    assert(cbs->on_new_frame);

    vb->buffering_ms = buffering_ms;
    vb->cbs = cbs;
    vb->cbs_userdata = cbs_userdata;
    return true;
}

bool
sc_video_buffer_start(struct sc_video_buffer *vb) {
    if (vb->buffering_ms) {
        bool ok =
            sc_thread_create(&vb->b.thread, run_buffering, "buffering", vb);
        if (!ok) {
            LOGE("Could not start buffering thread");
            return false;
        }
    }

    return true;
}

void
sc_video_buffer_stop(struct sc_video_buffer *vb) {
    if (vb->buffering_ms) {
        sc_mutex_lock(&vb->b.mutex);
        vb->b.stopped = true;
        sc_cond_signal(&vb->b.queue_cond);
        sc_cond_signal(&vb->b.wait_cond);
        sc_mutex_unlock(&vb->b.mutex);
    }
}

void
sc_video_buffer_join(struct sc_video_buffer *vb) {
    if (vb->buffering_ms) {
        sc_thread_join(&vb->b.thread, NULL);
    }
}

void
sc_video_buffer_destroy(struct sc_video_buffer *vb) {
    sc_frame_buffer_destroy(&vb->fb);
    if (vb->buffering_ms) {
        sc_cond_destroy(&vb->b.wait_cond);
        sc_cond_destroy(&vb->b.queue_cond);
        sc_mutex_destroy(&vb->b.mutex);
    }
}

bool
sc_video_buffer_push(struct sc_video_buffer *vb, const AVFrame *frame) {
    if (!vb->buffering_ms) {
        // no buffering
        return sc_video_buffer_offer(vb, frame);
    }

    struct sc_video_buffer_frame *vb_frame = sc_video_buffer_frame_new(frame);
    if (!vb_frame) {
        LOGE("Could not allocate frame");
        return false;
    }

    sc_clock_update(&vb->b.clock, sc_tick_now(), vb_frame->frame->pts);

    sc_mutex_lock(&vb->b.mutex);
    sc_queue_push(&vb->b.queue, next, vb_frame);
    sc_cond_signal(&vb->b.queue_cond);
    sc_cond_signal(&vb->b.wait_cond);
    sc_mutex_unlock(&vb->b.mutex);

    return true;
}

void
sc_video_buffer_consume(struct sc_video_buffer *vb, AVFrame *dst) {
    sc_frame_buffer_consume(&vb->fb, dst);
}
