#include "resizer.h"

#include <assert.h>
#include <libswscale/swscale.h>

#include "util/log.h"

bool
sc_resizer_init(struct sc_resizer *resizer, struct video_buffer *vb_in,
                enum sc_scale_filter scale_filter, struct size size) {
    bool ok = sc_mutex_init(&resizer->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&resizer->req_cond);
    if (!ok) {
        sc_mutex_destroy(&resizer->mutex);
        return false;
    }

    ok = video_buffer_init(&resizer->vb_out, false); // FIXME wait_consumer
    if (!ok) {
        sc_cond_destroy(&resizer->req_cond);
        sc_mutex_destroy(&resizer->mutex);
        return false;
    }

    resizer->resized_frame = av_frame_alloc();
    if (!resizer->resized_frame) {
        video_buffer_destroy(&resizer->vb_out);
        sc_cond_destroy(&resizer->req_cond);
        sc_mutex_destroy(&resizer->mutex);
        return false;
    }

    resizer->vb_in = vb_in;
    resizer->scale_filter = scale_filter;
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

static int
to_sws_filter(enum sc_scale_filter scale_filter) {
    switch (scale_filter) {
        case SC_SCALE_FILTER_BILINEAR:  return SWS_BILINEAR;
        case SC_SCALE_FILTER_BICUBIC:   return SWS_BICUBIC;
        case SC_SCALE_FILTER_X:         return SWS_X;
        case SC_SCALE_FILTER_POINT:     return SWS_POINT;
        case SC_SCALE_FILTER_AREA:      return SWS_AREA;
        case SC_SCALE_FILTER_BICUBLIN:  return SWS_BICUBLIN;
        case SC_SCALE_FILTER_GAUSS:     return SWS_GAUSS;
        case SC_SCALE_FILTER_SINC:      return SWS_SINC;
        case SC_SCALE_FILTER_LANCZOS:   return SWS_LANCZOS;
        case SC_SCALE_FILTER_SPLINE:    return SWS_SPLINE;
        default: assert(!"unsupported filter");
    }
}

static bool
sc_resizer_swscale(struct sc_resizer *resizer) {
    assert(!resizer->resized_frame->buf[0]); // The frame must be "empty"
    assert(resizer->size.width);
    assert(resizer->size.height);

    const AVFrame *in = resizer->input_frame;
    struct size size = resizer->size;

    AVFrame *out = resizer->resized_frame;
    out->format = AV_PIX_FMT_RGB24;
    out->width = size.width;
    out->height = size.height;

    int ret = av_frame_get_buffer(out, 32);
    if (ret < 0) {
        return false;
    }

    int flags = to_sws_filter(resizer->scale_filter);
    struct SwsContext *ctx =
        sws_getContext(in->width, in->height, AV_PIX_FMT_YUV420P,
                       size.width, size.height, AV_PIX_FMT_RGB24, flags, NULL,
                       NULL, NULL);
    if (!ctx) {
        av_frame_unref(out);
        return false;
    }

    sws_scale(ctx, (const uint8_t *const *) in->data, in->linesize, 0,
              in->height, out->data, out->linesize);
    sws_freeContext(ctx);

    return true;
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

        sc_mutex_lock(&resizer->mutex);

        // Update the original size of the resized frame
        resizer->original_size.width = resizer->input_frame->width;
        resizer->original_size.height = resizer->input_frame->height;
        assert(resizer->original_size.width);
        assert(resizer->original_size.height);

        video_buffer_producer_offer_frame(&resizer->vb_out,
                                          &resizer->resized_frame);
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

    video_buffer_interrupt(&resizer->vb_out);
}

void
sc_resizer_join(struct sc_resizer *resizer) {
    sc_thread_join(&resizer->thread, NULL);
}

const AVFrame *
sc_resizer_consumer_take_frame(struct sc_resizer *resizer,
                               struct size *out_original_size) {
    // Locking the mutex is necessary to ensure that the size corresponds to the correct frame
    sc_mutex_lock(&resizer->mutex);

    const AVFrame *frame = video_buffer_consumer_take_frame(&resizer->vb_out);
    *out_original_size = resizer->original_size;

    sc_mutex_unlock(&resizer->mutex);

    return frame;
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
