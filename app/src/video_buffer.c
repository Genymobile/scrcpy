#include "video_buffer.h"

#include <assert.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

bool
video_buffer_init(struct video_buffer *vb, bool wait_consumer) {
    vb->producer_frame = av_frame_alloc();
    if (!vb->producer_frame) {
        goto error_0;
    }

    vb->pending_frame = av_frame_alloc();
    if (!vb->pending_frame) {
        goto error_1;
    }

    vb->consumer_frame = av_frame_alloc();
    if (!vb->consumer_frame) {
        goto error_2;
    }

    bool ok = sc_mutex_init(&vb->mutex);
    if (!ok) {
        goto error_3;
    }

    vb->wait_consumer = wait_consumer;
    if (wait_consumer) {
        ok = sc_cond_init(&vb->pending_frame_consumed_cond);
        if (!ok) {
            sc_mutex_destroy(&vb->mutex);
            goto error_2;
        }
        // interrupted is not used if wait_consumer is disabled since offering
        // a frame will never block
        vb->interrupted = false;
    }

    // there is initially no frame, so consider it has already been consumed
    vb->pending_frame_consumed = true;

    // The callbacks must be set by the consumer via
    // video_buffer_set_consumer_callbacks()
    vb->cbs = NULL;

    return true;

error_3:
    av_frame_free(&vb->consumer_frame);
error_2:
    av_frame_free(&vb->pending_frame);
error_1:
    av_frame_free(&vb->producer_frame);
error_0:
    return false;
}

void
video_buffer_destroy(struct video_buffer *vb) {
    if (vb->wait_consumer) {
        sc_cond_destroy(&vb->pending_frame_consumed_cond);
    }
    sc_mutex_destroy(&vb->mutex);
    av_frame_free(&vb->consumer_frame);
    av_frame_free(&vb->pending_frame);
    av_frame_free(&vb->producer_frame);
}

static inline void
swap_frames(AVFrame **lhs, AVFrame **rhs) {
    AVFrame *tmp = *lhs;
    *lhs = *rhs;
    *rhs = tmp;
}

void
video_buffer_set_consumer_callbacks(struct video_buffer *vb,
                                    const struct video_buffer_callbacks *cbs,
                                    void *cbs_userdata) {
    assert(!vb->cbs); // must be set only once
    assert(cbs);
    assert(cbs->on_frame_available);
    vb->cbs = cbs;
    vb->cbs_userdata = cbs_userdata;
}

void
video_buffer_producer_offer_frame(struct video_buffer *vb) {
    assert(vb->cbs);

    sc_mutex_lock(&vb->mutex);
    if (vb->wait_consumer) {
        // wait for the current (expired) frame to be consumed
        while (!vb->pending_frame_consumed && !vb->interrupted) {
            sc_cond_wait(&vb->pending_frame_consumed_cond, &vb->mutex);
        }
    }

    swap_frames(&vb->producer_frame, &vb->pending_frame);

    bool skipped = !vb->pending_frame_consumed;
    vb->pending_frame_consumed = false;

    sc_mutex_unlock(&vb->mutex);

    if (skipped) {
        if (vb->cbs->on_frame_skipped)
            vb->cbs->on_frame_skipped(vb, vb->cbs_userdata);
    } else {
        vb->cbs->on_frame_available(vb, vb->cbs_userdata);
    }
}

const AVFrame *
video_buffer_consumer_take_frame(struct video_buffer *vb) {
    sc_mutex_lock(&vb->mutex);
    assert(!vb->pending_frame_consumed);
    vb->pending_frame_consumed = true;

    swap_frames(&vb->consumer_frame, &vb->pending_frame);

    if (vb->wait_consumer) {
        // unblock video_buffer_offer_decoded_frame()
        sc_cond_signal(&vb->pending_frame_consumed_cond);
    }
    sc_mutex_unlock(&vb->mutex);

    // consumer_frame is only written from this thread, no need to lock
    return vb->consumer_frame;
}

void
video_buffer_interrupt(struct video_buffer *vb) {
    if (vb->wait_consumer) {
        sc_mutex_lock(&vb->mutex);
        vb->interrupted = true;
        sc_mutex_unlock(&vb->mutex);
        // wake up blocking wait
        sc_cond_signal(&vb->pending_frame_consumed_cond);
    }
}
