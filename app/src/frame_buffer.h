#ifndef SC_FRAME_BUFFER_H
#define SC_FRAME_BUFFER_H

#include "common.h"

#include <stdbool.h>
#include <libavutil/frame.h>

#include "util/thread.h"

// forward declarations
typedef struct AVFrame AVFrame;

/**
 * A frame buffer holds 1 pending frame, which is the last frame received from
 * the producer (typically, the decoder).
 *
 * If a pending frame has not been consumed when the producer pushes a new
 * frame, then it is lost. The intent is to always provide access to the very
 * last frame to minimize latency.
 */

struct sc_frame_buffer {
    AVFrame *pending_frame;
    AVFrame *tmp_frame; // To preserve the pending frame on error

    sc_mutex mutex;

    bool pending_frame_consumed;
};

bool
sc_frame_buffer_init(struct sc_frame_buffer *fb);

void
sc_frame_buffer_destroy(struct sc_frame_buffer *fb);

bool
sc_frame_buffer_push(struct sc_frame_buffer *fb, const AVFrame *frame,
                     bool *skipped);

void
sc_frame_buffer_consume(struct sc_frame_buffer *fb, AVFrame *dst);

#endif
