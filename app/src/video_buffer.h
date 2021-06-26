#ifndef VIDEO_BUFFER_H
#define VIDEO_BUFFER_H

#include "common.h"

#include <stdbool.h>

#include "fps_counter.h"
#include "util/thread.h"

// forward declarations
typedef struct AVFrame AVFrame;

/**
 * A video buffer holds 1 pending frame, which is the last frame received from
 * the producer (typically, the decoder).
 *
 * If a pending frame has not been consumed when the producer pushes a new
 * frame, then it is lost. The intent is to always provide access to the very
 * last frame to minimize latency.
 */

struct video_buffer {
    AVFrame *pending_frame;
    AVFrame *tmp_frame; // To preserve the pending frame on error

    sc_mutex mutex;

    bool pending_frame_consumed;
};

bool
video_buffer_init(struct video_buffer *vb);

void
video_buffer_destroy(struct video_buffer *vb);

bool
video_buffer_push(struct video_buffer *vb, const AVFrame *frame, bool *skipped);

void
video_buffer_consume(struct video_buffer *vb, AVFrame *dst);

#endif
