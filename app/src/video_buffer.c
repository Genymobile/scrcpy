#include "video_buffer.h"

#include <assert.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

struct sc_clock {
    double coeff;
    sc_tick offset;
    unsigned range;

    struct {
        sc_tick system;
        sc_tick stream;
    } last;
};

static void
sc_clock_init(struct sc_clock *clock) {
    clock->coeff = 1;
    clock->offset = 0;
    clock->range = 0;

    clock->last.system = 0;
    clock->last.stream = 0;
}

static void
sc_clock_update(struct sc_clock *clock, sc_tick now, sc_tick stream_ts) {
    sc_tick system_delta = now - clock->last.system;
    sc_tick stream_delta = stream_ts - clock->last.stream;
    double instant_coeff = (double) system_delta / stream_delta;

}

static sc_tick
sc_clock_get_system_ts(struct sc_clock *clock, sc_tick stream_ts) {
    return (sc_tick) (stream_ts * clock->coeff) + clock->offset;
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
            // Flush queue
            while (!sc_queue_is_empty(&vb->b.queue)) {
                struct sc_video_buffer_frame *vb_frame;
                sc_queue_take(&vb->b.queue, next, &vb_frame);
                sc_video_buffer_frame_delete(vb_frame);
            }
            break;
        }

        struct sc_video_buffer_frame *vb_frame;
        sc_queue_take(&vb->b.queue, next, &vb_frame);

        sc_mutex_unlock(&vb->b.mutex);

        usleep(vb->buffering_ms * 1000);

        sc_video_buffer_offer(vb, vb_frame->frame);

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

    sc_mutex_lock(&vb->b.mutex);
    sc_queue_push(&vb->b.queue, next, vb_frame);
    sc_cond_signal(&vb->b.queue_cond);
    sc_mutex_unlock(&vb->b.mutex);

    return true;
}

void
sc_video_buffer_consume(struct sc_video_buffer *vb, AVFrame *dst) {
    sc_frame_buffer_consume(&vb->fb, dst);
}
