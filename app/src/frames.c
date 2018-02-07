#include "frames.h"

#include <SDL2/SDL_mutex.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

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
#endif

    frames->rendering_frame_consumed = SDL_TRUE;

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

void frames_swap(struct frames *frames) {
    AVFrame *tmp = frames->decoding_frame;
    frames->decoding_frame = frames->rendering_frame;
    frames->rendering_frame = tmp;
}
