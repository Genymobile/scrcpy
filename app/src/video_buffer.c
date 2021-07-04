#include "video_buffer.h"

#include <assert.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

bool
sc_video_buffer_init(struct sc_video_buffer *vb) {
    return sc_frame_buffer_init(&vb->fb);
}

void
sc_video_buffer_destroy(struct sc_video_buffer *vb) {
    sc_frame_buffer_destroy(&vb->fb);
}

bool
sc_video_buffer_push(struct sc_video_buffer *vb, const AVFrame *frame,
                  bool *previous_frame_skipped) {
    return sc_frame_buffer_push(&vb->fb, frame, previous_frame_skipped);
}

void
sc_video_buffer_consume(struct sc_video_buffer *vb, AVFrame *dst) {
    sc_frame_buffer_consume(&vb->fb, dst);
}
