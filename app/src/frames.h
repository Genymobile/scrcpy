#ifndef FRAMES_H
#define FRAMES_H

#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_stdinc.h>

#include "config.h"
#include "fpscounter.h"

// forward declarations
typedef struct AVFrame AVFrame;

struct frames {
    AVFrame *decoding_frame;
    AVFrame *rendering_frame;
    SDL_mutex *mutex;
#ifndef SKIP_FRAMES
    SDL_bool stopped;
    SDL_cond *rendering_frame_consumed_cond;
#endif
    SDL_bool rendering_frame_consumed;
    struct fps_counter fps_counter;
};

SDL_bool frames_init(struct frames *frames);
void frames_destroy(struct frames *frames);

// set the decoder frame as ready for rendering
// this function locks frames->mutex during its execution
// returns true if the previous frame had been consumed
SDL_bool frames_offer_decoded_frame(struct frames *frames);

// mark the rendering frame as consumed and return it
// MUST be called with frames->mutex locked!!!
// the caller is expected to render the returned frame to some texture before
// unlocking frames->mutex
const AVFrame *frames_consume_rendered_frame(struct frames *frames);

// wake up and avoid any blocking call
void frames_stop(struct frames *frames);

#endif
