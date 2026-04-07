#include "frame_buffer.h"

#include <assert.h>

#include "util/log.h"

bool
sc_frame_buffer_init(struct sc_frame_buffer *fb) {
    fb->pending_frame = av_frame_alloc();
    if (!fb->pending_frame) {
        LOG_OOM();
        return false;
    }

    fb->tmp_frame = av_frame_alloc();
    if (!fb->tmp_frame) {
        LOG_OOM();
        av_frame_free(&fb->pending_frame);
        return false;
    }

    // there is initially no frame, so consider it has already been consumed
    fb->pending_frame_consumed = true;

    return true;
}

void
sc_frame_buffer_destroy(struct sc_frame_buffer *fb) {
    av_frame_free(&fb->pending_frame);
    av_frame_free(&fb->tmp_frame);
}

static inline void
swap_frames(AVFrame **lhs, AVFrame **rhs) {
    AVFrame *tmp = *lhs;
    *lhs = *rhs;
    *rhs = tmp;
}

bool
sc_frame_buffer_has_frame(struct sc_frame_buffer *fb) {
    return !fb->pending_frame_consumed;
}

bool
sc_frame_buffer_push(struct sc_frame_buffer *fb, const AVFrame *frame) {
    // Use a temporary frame to preserve pending_frame in case of error.
    // tmp_frame is an empty frame, no need to call av_frame_unref() beforehand.
    int r = av_frame_ref(fb->tmp_frame, frame);
    if (r) {
        LOGE("Could not ref frame: %d", r);
        return false;
    }

    // Now that av_frame_ref() succeeded, we can replace the previous
    // pending_frame
    swap_frames(&fb->pending_frame, &fb->tmp_frame);
    av_frame_unref(fb->tmp_frame);

    fb->pending_frame_consumed = false;
    return true;
}

void
sc_frame_buffer_consume(struct sc_frame_buffer *fb, AVFrame *dst) {
    assert(!fb->pending_frame_consumed);
    fb->pending_frame_consumed = true;

    av_frame_move_ref(dst, fb->pending_frame);
    // av_frame_move_ref() resets its source frame, so no need to call
    // av_frame_unref()
}
