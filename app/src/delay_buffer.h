#ifndef SC_DELAY_BUFFER_H
#define SC_DELAY_BUFFER_H

#include "common.h"

#include <stdbool.h>
#include <libavutil/frame.h>

#include "clock.h"
#include "trait/frame_source.h"
#include "trait/frame_sink.h"
#include "util/thread.h"
#include "util/tick.h"
#include "util/vecdeque.h"

//#define SC_BUFFERING_DEBUG // uncomment to debug

// forward declarations
typedef struct AVFrame AVFrame;

struct sc_delayed_frame {
    AVFrame *frame;
#ifdef SC_BUFFERING_DEBUG
    sc_tick push_date;
#endif
};

struct sc_delayed_frame_queue SC_VECDEQUE(struct sc_delayed_frame);

struct sc_delay_buffer {
    struct sc_frame_source frame_source; // frame source trait
    struct sc_frame_sink frame_sink; // frame sink trait

    sc_tick delay;
    bool first_frame_asap;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond queue_cond;
    sc_cond wait_cond;

    struct sc_clock clock;
    struct sc_delayed_frame_queue queue;
    bool stopped;
};

struct sc_delay_buffer_callbacks {
    bool (*on_new_frame)(struct sc_delay_buffer *db, const AVFrame *frame,
                         void *userdata);
};

/**
 * Initialize a delay buffer.
 *
 * \param delay a (strictly) positive delay
 * \param first_frame_asap if true, do not delay the first frame (useful for
                           a video stream).
 */
void
sc_delay_buffer_init(struct sc_delay_buffer *db, sc_tick delay,
                     bool first_frame_asap);

#endif
