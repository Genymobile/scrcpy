#include "video_buffer.h"

#include <assert.h>
#include <SDL2/SDL_mutex.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "config.h"
#include "util/lock.h"
#include "util/log.h"

bool
video_buffer_init(struct video_buffer *vb, struct fps_counter *fps_counter,
                  bool render_expired_frames) {
    vb->fps_counter = fps_counter;

    if (!(vb->decoding_frame = av_frame_alloc())) {
        goto error_0;
    }

    if (!(vb->rendering_frame = av_frame_alloc())) {
        goto error_1;
    }

    if (!(vb->mutex = SDL_CreateMutex())) {
        goto error_2;
    }

    vb->render_expired_frames = render_expired_frames;
    if (render_expired_frames) {
        if (!(vb->rendering_frame_consumed_cond = SDL_CreateCond())) {
            SDL_DestroyMutex(vb->mutex);
            goto error_2;
        }
        // interrupted is not used if expired frames are not rendered
        // since offering a frame will never block
        vb->interrupted = false;
    }

    // there is initially no rendering frame, so consider it has already been
    // consumed
    vb->rendering_frame_consumed = true;

    return true;

error_2:
    av_frame_free(&vb->rendering_frame);
error_1:
    av_frame_free(&vb->decoding_frame);
error_0:
    return false;
}

void
video_buffer_destroy(struct video_buffer *vb) {
    if (vb->render_expired_frames) {
        SDL_DestroyCond(vb->rendering_frame_consumed_cond);
    }
    SDL_DestroyMutex(vb->mutex);
    av_frame_free(&vb->rendering_frame);
    av_frame_free(&vb->decoding_frame);
}

static void
video_buffer_swap_frames(struct video_buffer *vb) {
    AVFrame *tmp = vb->decoding_frame;
    vb->decoding_frame = vb->rendering_frame;
    vb->rendering_frame = tmp;
}

void
video_buffer_offer_decoded_frame(struct video_buffer *vb,
                                 bool *previous_frame_skipped) {
    mutex_lock(vb->mutex);
    if (vb->render_expired_frames) {
        // wait for the current (expired) frame to be consumed
        while (!vb->rendering_frame_consumed && !vb->interrupted) {
            cond_wait(vb->rendering_frame_consumed_cond, vb->mutex);
        }
    } else if (!vb->rendering_frame_consumed) {
        fps_counter_add_skipped_frame(vb->fps_counter);
    }

    video_buffer_swap_frames(vb);

    *previous_frame_skipped = !vb->rendering_frame_consumed;
    vb->rendering_frame_consumed = false;

    mutex_unlock(vb->mutex);
}

const AVFrame *
video_buffer_consume_rendered_frame(struct video_buffer *vb) {
    assert(!vb->rendering_frame_consumed);
    vb->rendering_frame_consumed = true;
    fps_counter_add_rendered_frame(vb->fps_counter);
    if (vb->render_expired_frames) {
        // unblock video_buffer_offer_decoded_frame()
        cond_signal(vb->rendering_frame_consumed_cond);
    }
    return vb->rendering_frame;
}

void
video_buffer_interrupt(struct video_buffer *vb) {
    if (vb->render_expired_frames) {
        mutex_lock(vb->mutex);
        vb->interrupted = true;
        mutex_unlock(vb->mutex);
        // wake up blocking wait
        cond_signal(vb->rendering_frame_consumed_cond);
    }
}
