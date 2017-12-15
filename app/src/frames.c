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

    if (!(frames->rendering_frame_consumed_cond = SDL_CreateCond())) {
        goto error_3;
    }

    frames->rendering_frame_consumed = SDL_TRUE;

    return SDL_TRUE;

error_3:
    SDL_DestroyMutex(frames->mutex);
error_2:
    av_frame_free(&frames->rendering_frame);
error_1:
    av_frame_free(&frames->decoding_frame);
error_0:
    return SDL_FALSE;
}

void frames_destroy(struct frames *frames) {
    SDL_DestroyMutex(frames->mutex);
    SDL_DestroyCond(frames->rendering_frame_consumed_cond);
    av_frame_free(&frames->rendering_frame);
    av_frame_free(&frames->decoding_frame);
}

void frames_swap(struct frames *frames) {
    AVFrame *tmp = frames->decoding_frame;
    frames->decoding_frame = frames->rendering_frame;
    frames->rendering_frame = tmp;
}
