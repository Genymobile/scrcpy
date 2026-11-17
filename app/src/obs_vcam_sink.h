#ifndef SC_OBS_VCAM_SINK_H
#define SC_OBS_VCAM_SINK_H

#include "common.h"

#include <stdbool.h>
#include <libavcodec/avcodec.h>

#include "frame_buffer.h"
#include "trait/frame_sink.h"
#include "util/thread.h"

/**
 * Frame sink writing to the shared memory read by the OBS Virtual Camera
 * DirectShow filter (Windows only)
 */
struct sc_obs_vcam_sink {
    struct sc_frame_sink frame_sink; // frame sink trait

    struct sc_frame_buffer fb;

    void *mapping; // HANDLE
    struct sc_obs_vcam_header *header;
    uint32_t capacity; // max frame data size per slot

    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    bool has_frame;
    bool stopped;

    AVFrame *frame;
};

void
sc_obs_vcam_sink_init(struct sc_obs_vcam_sink *vs);

#endif
