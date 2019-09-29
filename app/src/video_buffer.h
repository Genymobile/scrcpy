#ifndef VIDEO_BUFFER_H
#define VIDEO_BUFFER_H

#include <stdbool.h>
#include <SDL2/SDL_mutex.h>

#include "config.h"
#include "fps_counter.h"

// forward declarations
typedef struct AVFrame AVFrame;

struct video_buffer {
    AVFrame *decoding_frame;
    AVFrame *rendering_frame;
    SDL_mutex *mutex;
    bool render_expired_frames;
    bool interrupted;
    SDL_cond *rendering_frame_consumed_cond;
    bool rendering_frame_consumed;
    struct fps_counter *fps_counter;
};

bool
video_buffer_init(struct video_buffer *vb, struct fps_counter *fps_counter,
                  bool render_expired_frames);

void
video_buffer_destroy(struct video_buffer *vb);

// set the decoded frame as ready for rendering
// this function locks frames->mutex during its execution
// the output flag is set to report whether the previous frame has been skipped
void
video_buffer_offer_decoded_frame(struct video_buffer *vb,
                                 bool *previous_frame_skipped);

// mark the rendering frame as consumed and return it
// MUST be called with frames->mutex locked!!!
// the caller is expected to render the returned frame to some texture before
// unlocking frames->mutex
const AVFrame *
video_buffer_consume_rendered_frame(struct video_buffer *vb);

// wake up and avoid any blocking call
void
video_buffer_interrupt(struct video_buffer *vb);

#endif
