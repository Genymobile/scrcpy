#ifndef SC_DELAY_BUFFER_H
#define SC_DELAY_BUFFER_H

#include "common.h"

#include <stdbool.h>

#include "clock.h"
#include "util/thread.h"
#include "util/tick.h"
#include "util/vecdeque.h"

// forward declarations
typedef struct AVFrame AVFrame;

struct sc_delayed_frame {
    AVFrame *frame;
#ifndef NDEBUG
    sc_tick push_date;
#endif
};

struct sc_delayed_frame_queue SC_VECDEQUE(struct sc_delayed_frame);

struct sc_delay_buffer {
    sc_tick delay;

    // only if delay > 0
    struct {
        sc_thread thread;
        sc_mutex mutex;
        sc_cond queue_cond;
        sc_cond wait_cond;

        struct sc_clock clock;
        struct sc_delayed_frame_queue queue;
        bool stopped;
    } b; // buffering

    const struct sc_delay_buffer_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_delay_buffer_callbacks {
    bool (*on_new_frame)(struct sc_delay_buffer *db, const AVFrame *frame,
                         void *userdata);
};

bool
sc_delay_buffer_init(struct sc_delay_buffer *db, sc_tick delay,
                     const struct sc_delay_buffer_callbacks *cbs,
                     void *cbs_userdata);

bool
sc_delay_buffer_start(struct sc_delay_buffer *db);

void
sc_delay_buffer_stop(struct sc_delay_buffer *db);

void
sc_delay_buffer_join(struct sc_delay_buffer *db);

void
sc_delay_buffer_destroy(struct sc_delay_buffer *db);

bool
sc_delay_buffer_push(struct sc_delay_buffer *db, const AVFrame *frame);

#endif
