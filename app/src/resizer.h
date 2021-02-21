#ifndef SC_RESIZER
#define SC_RESIZER

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

#include "coords.h"
#include "scrcpy.h"
#include "util/thread.h"
#include "video_buffer.h"

struct sc_resizer {
    struct video_buffer *vb_in;
    struct video_buffer *vb_out;
    enum sc_scale_filter scale_filter;
    struct size size;

    // valid until the next call to video_buffer_consumer_take_frame(vb_in)
    const AVFrame *input_frame;
    AVFrame *resized_frame;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond req_cond;

    bool has_request;
    bool has_new_frame;
    bool interrupted;
};

bool
sc_resizer_init(struct sc_resizer *resizer, struct video_buffer *vb_in,
                struct video_buffer *vb_out, enum sc_scale_filter scale_filter,
                struct size size);

void
sc_resizer_destroy(struct sc_resizer *resizer);

bool
sc_resizer_start(struct sc_resizer *resizer);

void
sc_resizer_stop(struct sc_resizer *resizer);

void
sc_resizer_join(struct sc_resizer *resizer);

void
sc_resizer_process_new_frame(struct sc_resizer *resizer);

void
sc_resizer_process_new_size(struct sc_resizer *resizer, struct size size);

#endif
