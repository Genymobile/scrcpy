#include "video_buffer.h"

#include <assert.h>
#include <stdlib.h>

#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

#define SC_BUFFERING_NDEBUG // comment to debug

static struct sc_video_buffer_frame *
sc_video_buffer_frame_new(const AVFrame *frame) {
    struct sc_video_buffer_frame *vb_frame = malloc(sizeof(*vb_frame));
    if (!vb_frame) {
        LOG_OOM();
        return NULL;
    }

    vb_frame->frame = av_frame_alloc();
    if (!vb_frame->frame) {
        LOG_OOM();
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

    assert(vb->buffering_time > 0);

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

        sc_tick max_deadline = sc_tick_now() + vb->buffering_time;
        // PTS (written by the server) are expressed in microseconds
        sc_tick pts = SC_TICK_TO_US(vb_frame->frame->pts);

        bool timed_out = false;
        while (!vb->b.stopped && !timed_out) {
            sc_tick deadline = sc_clock_to_system_time(&vb->b.clock, pts)
                             + vb->buffering_time;
            if (deadline > max_deadline) {
                deadline = max_deadline;
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

#ifndef SC_BUFFERING_NDEBUG
        LOGD("Buffering: %" PRItick ";%" PRItick ";%" PRItick,
             pts, vb_frame->push_date, sc_tick_now());
#endif

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
sc_video_buffer_init(struct sc_video_buffer *vb, sc_tick buffering_time,
                     const struct sc_video_buffer_callbacks *cbs,
                     void *cbs_userdata) {
    bool ok = sc_frame_buffer_init(&vb->fb);
    if (!ok) {
        return false;
    }

    assert(buffering_time >= 0);
    if (buffering_time) {
        ok = sc_mutex_init(&vb->b.mutex);
        if (!ok) {
            sc_frame_buffer_destroy(&vb->fb);
            return false;
        }

        ok = sc_cond_init(&vb->b.queue_cond);
        if (!ok) {
            sc_mutex_destroy(&vb->b.mutex);
            sc_frame_buffer_destroy(&vb->fb);
            return false;
        }

        ok = sc_cond_init(&vb->b.wait_cond);
        if (!ok) {
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

    vb->buffering_time = buffering_time;
    vb->cbs = cbs;
    vb->cbs_userdata = cbs_userdata;
    return true;
}

bool
sc_video_buffer_start(struct sc_video_buffer *vb) {
    if (vb->buffering_time) {
        bool ok =
            sc_thread_create(&vb->b.thread, run_buffering, "scrcpy-vbuf", vb);
        if (!ok) {
            LOGE("Could not start buffering thread");
            return false;
        }
    }

    return true;
}

void
sc_video_buffer_stop(struct sc_video_buffer *vb) {
    if (vb->buffering_time) {
        sc_mutex_lock(&vb->b.mutex);
        vb->b.stopped = true;
        sc_cond_signal(&vb->b.queue_cond);
        sc_cond_signal(&vb->b.wait_cond);
        sc_mutex_unlock(&vb->b.mutex);
    }
}

void
sc_video_buffer_join(struct sc_video_buffer *vb) {
    if (vb->buffering_time) {
        sc_thread_join(&vb->b.thread, NULL);
    }
}

void
sc_video_buffer_destroy(struct sc_video_buffer *vb) {
    sc_frame_buffer_destroy(&vb->fb);
    if (vb->buffering_time) {
        sc_cond_destroy(&vb->b.wait_cond);
        sc_cond_destroy(&vb->b.queue_cond);
        sc_mutex_destroy(&vb->b.mutex);
    }
}

bool
sc_video_buffer_push(struct sc_video_buffer *vb, const AVFrame *frame) {
    if (!vb->buffering_time) {
        // No buffering
        return sc_video_buffer_offer(vb, frame);
    }

    sc_mutex_lock(&vb->b.mutex);

    sc_tick pts = SC_TICK_FROM_US(frame->pts);
    sc_clock_update(&vb->b.clock, sc_tick_now(), pts);
    sc_cond_signal(&vb->b.wait_cond);

    if (vb->b.clock.count == 1) {
        sc_mutex_unlock(&vb->b.mutex);
        // First frame, offer it immediately, for two reasons:
        //  - not to delay the opening of the scrcpy window
        //  - the buffering estimation needs at least two clock points, so it
        //  could not handle the first frame
        return sc_video_buffer_offer(vb, frame);
    }

    struct sc_video_buffer_frame *vb_frame = sc_video_buffer_frame_new(frame);
    if (!vb_frame) {
        sc_mutex_unlock(&vb->b.mutex);
        LOG_OOM();
        return false;
    }

#ifndef SC_BUFFERING_NDEBUG
    vb_frame->push_date = sc_tick_now();
#endif
    sc_queue_push(&vb->b.queue, next, vb_frame);
    sc_cond_signal(&vb->b.queue_cond);

    sc_mutex_unlock(&vb->b.mutex);

    return true;
}

void
sc_video_buffer_consume(struct sc_video_buffer *vb, AVFrame *dst) {
    sc_frame_buffer_consume(&vb->fb, dst);
}
