#include "frames.h"

#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_mutex.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "config.h"
#include "lockutil.h"
#include "log.h"

SDL_bool frames_init(struct frames *frames) {
    if (!(frames->decoding_frame = av_frame_alloc())) {
        goto error_0;
    }

    if (!(frames->rendering_frame = av_frame_alloc())) {
        goto error_1;
    }

    if (!(frames->mutex = SDL_CreateMutex())) {
        goto error_2;
    }

#ifndef SKIP_FRAMES
    if (!(frames->rendering_frame_consumed_cond = SDL_CreateCond())) {
        SDL_DestroyMutex(frames->mutex);
        goto error_2;
    }
    frames->stopped = SDL_FALSE;
#endif

    // there is initially no rendering frame, so consider it has already been
    // consumed
    frames->rendering_frame_consumed = SDL_TRUE;
    fps_counter_init(&frames->fps_counter);

    return SDL_TRUE;

error_2:
    av_frame_free(&frames->rendering_frame);
error_1:
    av_frame_free(&frames->decoding_frame);
error_0:
    return SDL_FALSE;
}

void frames_destroy(struct frames *frames) {
#ifndef SKIP_FRAMES
    SDL_DestroyCond(frames->rendering_frame_consumed_cond);
#endif
    SDL_DestroyMutex(frames->mutex);
    av_frame_free(&frames->rendering_frame);
    av_frame_free(&frames->decoding_frame);
}

static void frames_swap(struct frames *frames) {
    AVFrame *tmp = frames->decoding_frame;
    frames->decoding_frame = frames->rendering_frame;
    frames->rendering_frame = tmp;
}

SDL_bool frames_offer_decoded_frame(struct frames *frames) {
    mutex_lock(frames->mutex);
#ifndef SKIP_FRAMES
    // if SKIP_FRAMES is disabled, then the decoder must wait for the current
    // frame to be consumed
    while (!frames->rendering_frame_consumed && !frames->stopped) {
        cond_wait(frames->rendering_frame_consumed_cond, frames->mutex);
    }
#else
    if (frames->fps_counter.started && !frames->rendering_frame_consumed) {
        fps_counter_add_skipped_frame(&frames->fps_counter);
    }
#endif

    frames_swap(frames);

    SDL_bool previous_frame_consumed = frames->rendering_frame_consumed;
    frames->rendering_frame_consumed = SDL_FALSE;

    mutex_unlock(frames->mutex);
    return previous_frame_consumed;
}

const AVFrame *frames_consume_rendered_frame(struct frames *frames) {
    SDL_assert(!frames->rendering_frame_consumed);
    frames->rendering_frame_consumed = SDL_TRUE;
    if (frames->fps_counter.started) {
        fps_counter_add_rendered_frame(&frames->fps_counter);
    }
#ifndef SKIP_FRAMES
    // if SKIP_FRAMES is disabled, then notify the decoder the current frame is
    // consumed, so that it may push a new one
    cond_signal(frames->rendering_frame_consumed_cond);
#endif
    return frames->rendering_frame;
}

void frames_stop(struct frames *frames) {
#ifdef SKIP_FRAMES
    (void) frames; // unused
#else
    mutex_lock(frames->mutex);
    frames->stopped = SDL_TRUE;
    mutex_unlock(frames->mutex);
    // wake up blocking wait
    cond_signal(frames->rendering_frame_consumed_cond);
#endif
}
