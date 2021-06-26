#ifndef SC_VIDEO_BUFFER_H
#define SC_VIDEO_BUFFER_H

#include "common.h"

#include <stdbool.h>

#include "frame_buffer.h"

// forward declarations
typedef struct AVFrame AVFrame;

struct sc_video_buffer {
    struct sc_frame_buffer fb;
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
