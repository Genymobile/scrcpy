#ifndef SC_DAEMON_FRAME_KEEPER_H
#define SC_DAEMON_FRAME_KEEPER_H

#include "common.h"

#include <stdbool.h>
#include <libavutil/frame.h>

#include "coords.h"
#include "trait/frame_sink.h"
#include "util/thread.h"
#include "util/tick.h"

/**
 * Frame sink keeping a reference to the most recent decoded frame, readable
 * any number of times (unlike sc_frame_buffer, whose frames are
 * consume-once).
 *
 * Used by the daemon to serve screenshots on demand (doc/daemon.md §9.2).
 */
struct sc_frame_keeper {
    struct sc_frame_sink frame_sink; // frame sink trait

    sc_mutex mutex;
    sc_cond cond; // signaled on new frame, open and close
    AVFrame *latest; // most recent frame (NULL if none yet)
    AVFrame *tmp; // preserve latest on av_frame_ref() error
    sc_tick last_frame_tick; // when `latest` was received
    struct sc_size size; // dimensions of `latest` (0x0 if none yet)
    bool opened;
};

bool
sc_frame_keeper_init(struct sc_frame_keeper *fk);

void
sc_frame_keeper_destroy(struct sc_frame_keeper *fk);

/**
 * Get a reference to the latest frame into `dst` (an allocated, unreferenced
 * AVFrame owned by the caller), waiting until `deadline` if no frame was
 * received yet.
 *
 * On success, if `out_age` is not NULL, it receives the age of the frame.
 *
 * Returns false on timeout or if the sink is closed with no frame.
 */
bool
sc_frame_keeper_get(struct sc_frame_keeper *fk, AVFrame *dst,
                    sc_tick deadline, sc_tick *out_age);

/**
 * Wait until a frame newer than `min_tick` arrives (or `deadline` expires)
 * and get it. Used to implement freshness constraints after RESET_VIDEO.
 */
bool
sc_frame_keeper_get_since(struct sc_frame_keeper *fk, AVFrame *dst,
                          sc_tick min_tick, sc_tick deadline,
                          sc_tick *out_age);

/**
 * Tick of the most recent frame (0 if none received yet).
 */
sc_tick
sc_frame_keeper_last_tick(struct sc_frame_keeper *fk);

/**
 * Drop the retained frame (called between device sessions so that a stale
 * frame from a previous session is never served).
 */
void
sc_frame_keeper_reset(struct sc_frame_keeper *fk);

/**
 * Get the dimensions of the latest frame (the video size), waiting until
 * `deadline` if no frame was received yet.
 */
bool
sc_frame_keeper_wait_size(struct sc_frame_keeper *fk, sc_tick deadline,
                          struct sc_size *size);

#endif
