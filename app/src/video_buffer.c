#include "video_buffer.h"

#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_mutex.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "config.h"
#include "lock_util.h"
#include "log.h"

bool
video_buffer_init(struct video_buffer *vb) {
    if (!(vb->decoding_frame = av_frame_alloc())) {
        goto error_0;
    }

    if (!(vb->rendering_frame = av_frame_alloc())) {
        goto error_1;
    }

    if (!(vb->mutex = SDL_CreateMutex())) {
        goto error_2;
    }

#ifndef SKIP_FRAMES
    if (!(vb->rendering_frame_consumed_cond = SDL_CreateCond())) {
        SDL_DestroyMutex(vb->mutex);
        goto error_2;
    }
    vb->interrupted = false;
#endif

    // there is initially no rendering frame, so consider it has already been
    // consumed
    vb->rendering_frame_consumed = true;
    fps_counter_init(&vb->fps_counter);

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
#ifndef SKIP_FRAMES
    SDL_DestroyCond(vb->rendering_frame_consumed_cond);
#endif
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
#ifndef SKIP_FRAMES
    // if SKIP_FRAMES is disabled, then the decoder must wait for the current
    // frame to be consumed
    while (!vb->rendering_frame_consumed && !vb->interrupted) {
        cond_wait(vb->rendering_frame_consumed_cond, vb->mutex);
    }
#else
    if (vb->fps_counter.started && !vb->rendering_frame_consumed) {
        fps_counter_add_skipped_frame(&vb->fps_counter);
    }
#endif

    video_buffer_swap_frames(vb);

    *previous_frame_skipped = !vb->rendering_frame_consumed;
    vb->rendering_frame_consumed = false;

    mutex_unlock(vb->mutex);
}

const AVFrame *
video_buffer_consume_rendered_frame(struct video_buffer *vb) {
    SDL_assert(!vb->rendering_frame_consumed);
    vb->rendering_frame_consumed = true;
    if (vb->fps_counter.started) {
        fps_counter_add_rendered_frame(&vb->fps_counter);
    }
#ifndef SKIP_FRAMES
    // if SKIP_FRAMES is disabled, then notify the decoder the current frame is
    // consumed, so that it may push a new one
    cond_signal(vb->rendering_frame_consumed_cond);
#endif
    return vb->rendering_frame;
}

void
video_buffer_interrupt(struct video_buffer *vb) {
#ifdef SKIP_FRAMES
    (void) vb; // unused
#else
    mutex_lock(vb->mutex);
    vb->interrupted = true;
    mutex_unlock(vb->mutex);
    // wake up blocking wait
    cond_signal(vb->rendering_frame_consumed_cond);
#endif
}
