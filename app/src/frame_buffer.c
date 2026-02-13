#include "frame_buffer.h"

#include <assert.h>
#include <stdatomic.h>

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

    atomic_init(&fb->pending_frame_consumed, 1);

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
sc_frame_buffer_push(struct sc_frame_buffer *fb, const AVFrame *frame,
                     bool *previous_frame_skipped) {
    int r = av_frame_ref(fb->tmp_frame, frame);
    if (__builtin_expect(r != 0, 0)) {
        LOGE("Could not ref frame: %d", r);
        return false;
    }

    int was_consumed = atomic_exchange_explicit(&fb->pending_frame_consumed, 0, memory_order_acq_rel);
    
    if (previous_frame_skipped) {
        *previous_frame_skipped = !was_consumed;
    }

    swap_frames(&fb->pending_frame, &fb->tmp_frame);
    av_frame_unref(fb->tmp_frame);

    return true;
}

void
sc_frame_buffer_consume(struct sc_frame_buffer *fb, AVFrame *dst) {
    int expected = 0;
    if (!atomic_compare_exchange_strong_explicit(&fb->pending_frame_consumed, &expected, 1, 
                                                  memory_order_acq_rel, memory_order_acquire)) {
        assert(false && "Frame already consumed");
    }

    av_frame_move_ref(dst, fb->pending_frame);
}
