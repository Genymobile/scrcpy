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
 *
 * The producer and the consumer typically do not live in the same thread.
 * That's the reason why the callback on_frame_available() does not provide the
 * frame as parameter: the consumer might post an event to its own thread to
 * retrieve the pending frame from there, and that frame may have changed since
 * the callback if producer pushed a new one in between.
 */

struct video_buffer {
    AVFrame *pending_frame;
    AVFrame *tmp_frame; // To preserve the pending frame on error

    sc_mutex mutex;

    bool pending_frame_consumed;

    const struct video_buffer_callbacks *cbs;
    void *cbs_userdata;
};

struct video_buffer_callbacks {
    // Called when a new frame can be consumed.
    // This callback is mandatory (it must not be NULL).
    void (*on_frame_available)(struct video_buffer *vb, void *userdata);

    // Called when a pending frame has been overwritten by the producer.
    // This callback is optional (it may be NULL).
    void (*on_frame_skipped)(struct video_buffer *vb, void *userdata);
};

bool
video_buffer_init(struct video_buffer *vb);

void
video_buffer_destroy(struct video_buffer *vb);

void
video_buffer_set_consumer_callbacks(struct video_buffer *vb,
                                    const struct video_buffer_callbacks *cbs,
                                    void *cbs_userdata);

bool
video_buffer_push(struct video_buffer *vb, const AVFrame *frame);

void
video_buffer_consume(struct video_buffer *vb, AVFrame *dst);

#endif
