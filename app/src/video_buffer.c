#include "video_buffer.h"

#include <assert.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

bool
video_buffer_init(struct video_buffer *vb) {
    vb->pending_frame = av_frame_alloc();
    if (!vb->pending_frame) {
        return false;
    }

    vb->tmp_frame = av_frame_alloc();
    if (!vb->tmp_frame) {
        av_frame_free(&vb->pending_frame);
        return false;
    }

    bool ok = sc_mutex_init(&vb->mutex);
    if (!ok) {
        av_frame_free(&vb->pending_frame);
        av_frame_free(&vb->tmp_frame);
        return false;
    }

    // there is initially no frame, so consider it has already been consumed
    vb->pending_frame_consumed = true;

    // The callbacks must be set by the consumer via
    // video_buffer_set_consumer_callbacks()
    vb->cbs = NULL;

    return true;
}

void
video_buffer_destroy(struct video_buffer *vb) {
    sc_mutex_destroy(&vb->mutex);
    av_frame_free(&vb->pending_frame);
    av_frame_free(&vb->tmp_frame);
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

bool
video_buffer_push(struct video_buffer *vb, const AVFrame *frame) {
    assert(vb->cbs);

    sc_mutex_lock(&vb->mutex);

    // Use a temporary frame to preserve pending_frame in case of error.
    // tmp_frame is an empty frame, no need to call av_frame_unref() beforehand.
    int r = av_frame_ref(vb->tmp_frame, frame);
    if (r) {
        LOGE("Could not ref frame: %d", r);
        return false;
    }

    // Now that av_frame_ref() succeeded, we can replace the previous
    // pending_frame
    swap_frames(&vb->pending_frame, &vb->tmp_frame);
    av_frame_unref(vb->tmp_frame);

    bool skipped = !vb->pending_frame_consumed;
    vb->pending_frame_consumed = false;

    sc_mutex_unlock(&vb->mutex);

    if (skipped) {
        if (vb->cbs->on_frame_skipped)
            vb->cbs->on_frame_skipped(vb, vb->cbs_userdata);
    } else {
        vb->cbs->on_frame_available(vb, vb->cbs_userdata);
    }

    return true;
}

void
video_buffer_consume(struct video_buffer *vb, AVFrame *dst) {
    sc_mutex_lock(&vb->mutex);
    assert(!vb->pending_frame_consumed);
    vb->pending_frame_consumed = true;

    av_frame_move_ref(dst, vb->pending_frame);
    // av_frame_move_ref() resets its source frame, so no need to call
    // av_frame_unref()

    sc_mutex_unlock(&vb->mutex);
}
