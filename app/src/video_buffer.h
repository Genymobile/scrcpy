#ifndef SC_VIDEO_BUFFER_H
#define SC_VIDEO_BUFFER_H

#include "common.h"

#include <stdbool.h>

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

struct sc_video_buffer {
    AVFrame *pending_frame;
    AVFrame *tmp_frame; // To preserve the pending frame on error

    sc_mutex mutex;

    bool pending_frame_consumed;
};

bool
sc_video_buffer_init(struct sc_video_buffer *vb);

void
sc_video_buffer_destroy(struct sc_video_buffer *vb);

bool
sc_video_buffer_push(struct sc_video_buffer *vb, const AVFrame *frame,
                     bool *skipped);

void
sc_video_buffer_consume(struct sc_video_buffer *vb, AVFrame *dst);

#endif
