#ifndef FRAMES_H
#define FRAMES_H

#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_stdinc.h>

#include "config.h"

// forward declarations
typedef struct AVFrame AVFrame;

struct frames {
    AVFrame *decoding_frame;
    AVFrame *rendering_frame;
    SDL_mutex *mutex;
#ifndef SKIP_FRAMES
    SDL_cond *rendering_frame_consumed_cond;
#endif
    SDL_bool rendering_frame_consumed;
};

SDL_bool frames_init(struct frames *frames);
void frames_destroy(struct frames *frames);

void frames_swap(struct frames *frames);

#endif
