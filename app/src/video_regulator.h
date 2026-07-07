#ifndef SC_VIDEO_REGULATOR_H
#define SC_VIDEO_REGULATOR_H

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

enum sc_delayed_packet_type {
    SC_DELAYED_PACKET_TYPE_FRAME,
    SC_DELAYED_PACKET_TYPE_SESSION,
};

struct sc_delayed_packet {
    enum sc_delayed_packet_type type;
    union {
        AVFrame *frame;
        struct sc_stream_session session;
    };
#ifdef SC_BUFFERING_DEBUG
    sc_tick push_date;
#endif
};

struct sc_delayed_packet_queue SC_VECDEQUE(struct sc_delayed_packet);

struct sc_video_regulator {
    struct sc_frame_source frame_source; // frame source trait
    struct sc_frame_sink frame_sink; // frame sink trait

    sc_tick delay;
    bool first_frame_asap;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond queue_cond;
    sc_cond wait_cond;

    struct sc_clock clock;
    struct sc_delayed_packet_queue queue;
    bool stopped;
};

struct sc_video_regulator_callbacks {
    bool (*on_new_frame)(struct sc_video_regulator *vr, const AVFrame *frame,
                         void *userdata);
};

/**
 * Initialize a video regulator.
 *
 * \param delay a (strictly) positive delay
 * \param first_frame_asap if true, do not delay the first frame (useful for
                           a video stream).
 */
void
sc_video_regulator_init(struct sc_video_regulator *vr, sc_tick delay,
                        bool first_frame_asap);

#endif
