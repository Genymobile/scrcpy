#include "resizer.h"

#include <assert.h>

#include "util/log.h"

bool
sc_resizer_init(struct sc_resizer *resizer, struct video_buffer *vb_in,
                struct video_buffer *vb_out, struct size size) {
    bool ok = sc_mutex_init(&resizer->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&resizer->req_cond);
    if (!ok) {
        sc_mutex_destroy(&resizer->mutex);
        return false;
    }

    resizer->resized_frame = av_frame_alloc();
    if (!resizer->resized_frame) {
        sc_cond_destroy(&resizer->req_cond);
        sc_mutex_destroy(&resizer->mutex);
        return false;
    }

    resizer->vb_in = vb_in;
    resizer->vb_out = vb_out;
    resizer->size = size;

    resizer->input_frame = NULL;
    resizer->has_request = false;
    resizer->has_new_frame = false;
    resizer->interrupted = false;

    return true;
}

void
sc_resizer_destroy(struct sc_resizer *resizer) {
    av_frame_free(&resizer->resized_frame);
    sc_cond_destroy(&resizer->req_cond);
    sc_mutex_destroy(&resizer->mutex);
}

static bool
sc_resizer_swscale(struct sc_resizer *resizer) {
    assert(!resizer->resized_frame->buf[0]); // The frame must be "empty"

    return false;
}

static int
run_resizer(void *data) {
    struct sc_resizer *resizer = data;

    sc_mutex_lock(&resizer->mutex);
    while (!resizer->interrupted) {
        while (!resizer->interrupted && !resizer->has_request) {
            sc_cond_wait(&resizer->req_cond, &resizer->mutex);
        }

        if (resizer->has_new_frame) {
            resizer->input_frame =
                video_buffer_consumer_take_frame(resizer->vb_in);

            resizer->has_new_frame = false;
        }

        resizer->has_request = false;
        sc_mutex_unlock(&resizer->mutex);

        // Do the actual work without mutex
        sc_resizer_swscale(resizer);

        video_buffer_producer_offer_frame(resizer->vb_out,
                                          &resizer->resized_frame);

        sc_mutex_lock(&resizer->mutex);
    }
    sc_mutex_unlock(&resizer->mutex);

    return 0;
}

bool
sc_resizer_start(struct sc_resizer *resizer) {
    LOGD("Starting resizer thread");

    bool ok = sc_thread_create(&resizer->thread, run_resizer, "resizer",
                               resizer);
    if (!ok) {
        LOGE("Could not start resizer thread");
        return false;
    }

    return true;
}

void
sc_resizer_stop(struct sc_resizer *resizer) {
    sc_mutex_lock(&resizer->mutex);
    resizer->interrupted = true;
    sc_cond_signal(&resizer->req_cond);
    sc_mutex_unlock(&resizer->mutex);

    video_buffer_interrupt(resizer->vb_out);
}

void
sc_resizer_join(struct sc_resizer *resizer) {
    sc_thread_join(&resizer->thread, NULL);
}

void
sc_resizer_process_new_frame(struct sc_resizer *resizer) {
    sc_mutex_lock(&resizer->mutex);
    resizer->has_request = true;
    resizer->has_new_frame = true;
    sc_cond_signal(&resizer->req_cond);
    sc_mutex_unlock(&resizer->mutex);
}

void
sc_resizer_process_new_size(struct sc_resizer *resizer, struct size size) {
    sc_mutex_lock(&resizer->mutex);
    resizer->size = size;
    resizer->has_request = true;
    sc_cond_signal(&resizer->req_cond);
    sc_mutex_unlock(&resizer->mutex);
}
