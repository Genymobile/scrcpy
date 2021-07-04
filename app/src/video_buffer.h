#ifndef SC_VIDEO_BUFFER_H
#define SC_VIDEO_BUFFER_H

#include "common.h"

#include <stdbool.h>

#include "frame_buffer.h"
#include "util/queue.h"
#include "util/thread.h"

// forward declarations
typedef struct AVFrame AVFrame;

struct sc_video_buffer_frame {
    AVFrame *frame;
    sc_tick system_pts;
    struct sc_video_buffer_frame *next;
};

struct sc_video_buffer_frame_queue SC_QUEUE(struct sc_video_buffer_frame);

#define SC_CLOCK_RANGE 32

struct sc_clock_point {
    sc_tick system;
    sc_tick stream;
};

struct sc_clock_history {
    struct sc_clock_point points[SC_CLOCK_RANGE];
    unsigned count;
    unsigned head;

    sc_tick system_sum;
    sc_tick stream_sum;
};

struct sc_clock {
    double coeff;
    sc_tick offset;
    unsigned weight; // 0 <= weight && weight <= SC_CLOCK_RANGE

    struct {
        sc_tick system;
        sc_tick stream;
    } last;

    struct sc_clock_history history;
};

struct sc_video_buffer {
    struct sc_frame_buffer fb;

    unsigned buffering_ms;

    // only if buffering_ms > 0
    struct {
        sc_thread thread;
        sc_mutex mutex;
        sc_cond queue_cond;
        sc_cond wait_cond;

        struct sc_clock clock;
        struct sc_video_buffer_frame_queue queue;
        bool stopped;
    } b; // buffering

    const struct sc_video_buffer_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_video_buffer_callbacks {
    void (*on_new_frame)(struct sc_video_buffer *vb, bool previous_skipped,
                         void *userdata);
};

bool
sc_video_buffer_init(struct sc_video_buffer *vb, unsigned buffering_ms,
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
