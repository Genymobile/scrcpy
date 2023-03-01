#ifndef SC_VIDEO_BUFFER_H
#define SC_VIDEO_BUFFER_H

#include "common.h"

#include <stdbool.h>

#include "delay_buffer.h"
#include "frame_buffer.h"

struct sc_video_buffer {
    struct sc_delay_buffer db;
    struct sc_frame_buffer fb;

    const struct sc_video_buffer_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_video_buffer_callbacks {
    bool (*on_new_frame)(struct sc_video_buffer *vb, bool previous_skipped,
                         void *userdata);
};

bool
sc_video_buffer_init(struct sc_video_buffer *vb, sc_tick delay,
                     const struct sc_video_buffer_callbacks *cbs,
                     void *cbs_userdata);

bool
sc_video_buffer_start(struct sc_video_buffer *vb);

void
sc_video_buffer_stop(struct sc_video_buffer *vb);

void
sc_video_buffer_join(struct sc_video_buffer *vb);

void
sc_video_buffer_destroy(struct sc_video_buffer *vb);

bool
sc_video_buffer_push(struct sc_video_buffer *vb, const AVFrame *frame);

void
sc_video_buffer_consume(struct sc_video_buffer *vb, AVFrame *dst);

#endif
