#include "delay_buffer.h"

#include <assert.h>
#include <stdlib.h>

#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

#define SC_BUFFERING_NDEBUG // comment to debug

static bool
sc_delayed_frame_init(struct sc_delayed_frame *dframe, const AVFrame *frame) {
    dframe->frame = av_frame_alloc();
    if (!dframe->frame) {
        LOG_OOM();
        return false;
    }

    if (av_frame_ref(dframe->frame, frame)) {
        LOG_OOM();
        av_frame_free(&dframe->frame);
        return false;
    }

    return true;
}

static void
sc_delayed_frame_destroy(struct sc_delayed_frame *dframe) {
    av_frame_unref(dframe->frame);
    av_frame_free(&dframe->frame);
}

static bool
sc_delay_buffer_offer(struct sc_delay_buffer *db, const AVFrame *frame) {
    return db->cbs->on_new_frame(db, frame, db->cbs_userdata);
}

static int
run_buffering(void *data) {
    struct sc_delay_buffer *db = data;

    assert(db->delay > 0);

    for (;;) {
        sc_mutex_lock(&db->b.mutex);

        while (!db->b.stopped && sc_vecdeque_is_empty(&db->b.queue)) {
            sc_cond_wait(&db->b.queue_cond, &db->b.mutex);
        }

        if (db->b.stopped) {
            sc_mutex_unlock(&db->b.mutex);
            goto stopped;
        }

        struct sc_delayed_frame dframe = sc_vecdeque_pop(&db->b.queue);

        sc_tick max_deadline = sc_tick_now() + db->delay;
        // PTS (written by the server) are expressed in microseconds
        sc_tick pts = SC_TICK_FROM_US(dframe.frame->pts);

        bool timed_out = false;
        while (!db->b.stopped && !timed_out) {
            sc_tick deadline = sc_clock_to_system_time(&db->b.clock, pts)
                             + db->delay;
            if (deadline > max_deadline) {
                deadline = max_deadline;
            }

            timed_out =
                !sc_cond_timedwait(&db->b.wait_cond, &db->b.mutex, deadline);
        }

        bool stopped = db->b.stopped;
        sc_mutex_unlock(&db->b.mutex);

        if (stopped) {
            sc_delayed_frame_destroy(&dframe);
            goto stopped;
        }

#ifndef SC_BUFFERING_NDEBUG
        LOGD("Buffering: %" PRItick ";%" PRItick ";%" PRItick,
             pts, dframe.push_date, sc_tick_now());
#endif

        bool ok = sc_delay_buffer_offer(db, dframe.frame);
        sc_delayed_frame_destroy(&dframe);
        if (!ok) {
            LOGE("Delayed frame could not be pushed, stopping");
            sc_mutex_lock(&db->b.mutex);
            // Prevent to push any new packet
            db->b.stopped = true;
            sc_mutex_unlock(&db->b.mutex);
            goto stopped;
        }
    }

stopped:
    assert(db->b.stopped);

    // Flush queue
    while (!sc_vecdeque_is_empty(&db->b.queue)) {
        struct sc_delayed_frame *dframe = sc_vecdeque_popref(&db->b.queue);
        sc_delayed_frame_destroy(dframe);
    }

    LOGD("Buffering thread ended");

    return 0;
}

bool
sc_delay_buffer_init(struct sc_delay_buffer *db, sc_tick delay,
                     const struct sc_delay_buffer_callbacks *cbs,
                     void *cbs_userdata) {
    assert(delay >= 0);

    if (delay) {
        bool ok = sc_mutex_init(&db->b.mutex);
        if (!ok) {
            return false;
        }

        ok = sc_cond_init(&db->b.queue_cond);
        if (!ok) {
            sc_mutex_destroy(&db->b.mutex);
            return false;
        }

        ok = sc_cond_init(&db->b.wait_cond);
        if (!ok) {
            sc_cond_destroy(&db->b.queue_cond);
            sc_mutex_destroy(&db->b.mutex);
            return false;
        }

        sc_clock_init(&db->b.clock);
        sc_vecdeque_init(&db->b.queue);
    }

    assert(cbs);
    assert(cbs->on_new_frame);

    db->delay = delay;
    db->cbs = cbs;
    db->cbs_userdata = cbs_userdata;

    return true;
}

bool
sc_delay_buffer_start(struct sc_delay_buffer *db) {
    if (db->delay) {
        bool ok =
            sc_thread_create(&db->b.thread, run_buffering, "scrcpy-dbuf", db);
        if (!ok) {
            LOGE("Could not start buffering thread");
            return false;
        }
    }

    return true;
}

void
sc_delay_buffer_stop(struct sc_delay_buffer *db) {
    if (db->delay) {
        sc_mutex_lock(&db->b.mutex);
        db->b.stopped = true;
        sc_cond_signal(&db->b.queue_cond);
        sc_cond_signal(&db->b.wait_cond);
        sc_mutex_unlock(&db->b.mutex);
    }
}

void
sc_delay_buffer_join(struct sc_delay_buffer *db) {
    if (db->delay) {
        sc_thread_join(&db->b.thread, NULL);
    }
}

void
sc_delay_buffer_destroy(struct sc_delay_buffer *db) {
    if (db->delay) {
        sc_cond_destroy(&db->b.wait_cond);
        sc_cond_destroy(&db->b.queue_cond);
        sc_mutex_destroy(&db->b.mutex);
    }
}

bool
sc_delay_buffer_push(struct sc_delay_buffer *db, const AVFrame *frame) {
    if (!db->delay) {
        // No buffering
        return sc_delay_buffer_offer(db, frame);
    }

    sc_mutex_lock(&db->b.mutex);

    if (db->b.stopped) {
        sc_mutex_unlock(&db->b.mutex);
        return false;
    }

    sc_tick pts = SC_TICK_FROM_US(frame->pts);
    sc_clock_update(&db->b.clock, sc_tick_now(), pts);
    sc_cond_signal(&db->b.wait_cond);

    if (db->b.clock.count == 1) {
        sc_mutex_unlock(&db->b.mutex);
        // First frame, offer it immediately, for two reasons:
        //  - not to delay the opening of the scrcpy window
        //  - the buffering estimation needs at least two clock points, so it
        //  could not handle the first frame
        return sc_delay_buffer_offer(db, frame);
    }

    struct sc_delayed_frame dframe;
    bool ok = sc_delayed_frame_init(&dframe, frame);
    if (!ok) {
        sc_mutex_unlock(&db->b.mutex);
        return false;
    }

#ifndef SC_BUFFERING_NDEBUG
    dframe.push_date = sc_tick_now();
#endif

    ok = sc_vecdeque_push(&db->b.queue, dframe);
    if (!ok) {
        sc_mutex_unlock(&db->b.mutex);
        LOG_OOM();
        return false;
    }

    sc_cond_signal(&db->b.queue_cond);

    sc_mutex_unlock(&db->b.mutex);

    return true;
}
