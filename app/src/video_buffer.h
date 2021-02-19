#ifndef VIDEO_BUFFER_H
#define VIDEO_BUFFER_H

#include "common.h"

#include <stdbool.h>

#include "fps_counter.h"
#include "util/thread.h"

// forward declarations
typedef struct AVFrame AVFrame;

/**
 * There are 3 frames in memory:
 *  - one frame is held by the decoder (decoding_frame)
 *  - one frame is held by the renderer (rendering_frame)
 *  - one frame is shared between the decoder and the renderer (pending_frame)
 *
 * The decoder decodes a packet into the decoding_frame (it may takes time).
 *
 * Once the frame is decoded, it calls video_buffer_offer_decoded_frame(),
 * which swaps the decoding and pending frames.
 *
 * When the renderer is notified that a new frame is available, it calls
 * video_buffer_take_rendering_frame() to retrieve it, which swaps the pending
 * and rendering frames. The frame is valid until the next call, without
 * blocking the decoder.
 */

struct video_buffer {
    AVFrame *decoding_frame;
    AVFrame *pending_frame;
    AVFrame *rendering_frame;

    sc_mutex mutex;
    bool render_expired_frames;
    bool interrupted;

    sc_cond pending_frame_consumed_cond;
    bool pending_frame_consumed;
};

bool
video_buffer_init(struct video_buffer *vb, bool render_expired_frames);

void
video_buffer_destroy(struct video_buffer *vb);

// set the decoded frame as ready for rendering
// the output flag is set to report whether the previous frame has been skipped
void
video_buffer_offer_decoded_frame(struct video_buffer *vb,
                                 bool *previous_frame_skipped);

// mark the rendering frame as consumed and return it
// the frame is valid until the next call to this function
const AVFrame *
video_buffer_take_rendering_frame(struct video_buffer *vb);

// wake up and avoid any blocking call
void
video_buffer_interrupt(struct video_buffer *vb);

#endif
