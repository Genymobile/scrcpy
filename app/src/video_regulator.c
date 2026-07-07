#include "video_regulator.h"

#include <assert.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>

#include "util/log.h"

/** Downcast frame_sink to sc_video_regulator */
#define DOWNCAST(SINK) container_of(SINK, struct sc_video_regulator, frame_sink)

static bool
sc_delayed_packet_init_frame(struct sc_delayed_packet *dpacket,
                             const AVFrame *frame) {
    dpacket->type = SC_DELAYED_PACKET_TYPE_FRAME;
    dpacket->frame = av_frame_alloc();
    if (!dpacket->frame) {
        LOG_OOM();
        return false;
    }

    if (av_frame_ref(dpacket->frame, frame)) {
        LOG_OOM();
        av_frame_free(&dpacket->frame);
        return false;
    }

    return true;
}

static void
sc_delayed_packet_init_session(struct sc_delayed_packet *dpacket,
                               const struct sc_stream_session *session) {
    dpacket->type = SC_DELAYED_PACKET_TYPE_SESSION;
    dpacket->session = *session;
}

static void
sc_delayed_packet_destroy(struct sc_delayed_packet *dpacket) {
    if (dpacket->type == SC_DELAYED_PACKET_TYPE_FRAME) {
        av_frame_unref(dpacket->frame);
        av_frame_free(&dpacket->frame);
    }
}

static int
run_buffering(void *data) {
    struct sc_video_regulator *vr = data;

    assert(vr->delay > 0);

    for (;;) {
        sc_mutex_lock(&vr->mutex);

        while (!vr->stopped && sc_vecdeque_is_empty(&vr->queue)) {
            sc_cond_wait(&vr->queue_cond, &vr->mutex);
        }

        if (vr->stopped) {
            sc_mutex_unlock(&vr->mutex);
            goto stopped;
        }

        struct sc_delayed_packet dpacket = sc_vecdeque_pop(&vr->queue);

        bool ok;
        if (dpacket.type == SC_DELAYED_PACKET_TYPE_FRAME) {
            sc_tick max_deadline = sc_tick_now() + vr->delay;
            // PTS (written by the server) are expressed in microseconds
            sc_tick pts = SC_TICK_FROM_US(dpacket.frame->pts);

            bool timed_out = false;
            while (!vr->stopped && !timed_out) {
                sc_tick deadline = sc_clock_to_system_time(&vr->clock, pts)
                                 + vr->delay;
                if (deadline > max_deadline) {
                    deadline = max_deadline;
                }

                timed_out =
                    !sc_cond_timedwait(&vr->wait_cond, &vr->mutex, deadline);
            }

            bool stopped = vr->stopped;
            sc_mutex_unlock(&vr->mutex);

            if (stopped) {
                sc_delayed_packet_destroy(&dpacket);
                goto stopped;
            }

#ifdef SC_BUFFERING_DEBUG
            LOGD("Buffering: %" PRItick ";%" PRItick ";%" PRItick,
                 pts, dframe.push_date, sc_tick_now());
#endif

            ok = sc_frame_source_sinks_push(&vr->frame_source, dpacket.frame);
        } else {
            assert(dpacket.type == SC_DELAYED_PACKET_TYPE_SESSION);
            sc_mutex_unlock(&vr->mutex);
            ok = sc_frame_source_sinks_push_session(&vr->frame_source,
                                                    &dpacket.session);
        }

        sc_delayed_packet_destroy(&dpacket);
        if (!ok) {
            LOGE("Delayed packet could not be pushed, stopping");
            sc_mutex_lock(&vr->mutex);
            // Prevent to push any new packet
            vr->stopped = true;
            sc_mutex_unlock(&vr->mutex);
            goto stopped;
        }
    }

stopped:
    assert(vr->stopped);

    // Flush queue
    while (!sc_vecdeque_is_empty(&vr->queue)) {
        struct sc_delayed_packet *dpacket = sc_vecdeque_popref(&vr->queue);
        sc_delayed_packet_destroy(dpacket);
    }

    LOGD("Buffering thread ended");

    return 0;
}

static bool
sc_video_regulator_frame_sink_open(struct sc_frame_sink *sink,
                                   const AVCodecContext *ctx,
                                   const struct sc_stream_session *session) {
    struct sc_video_regulator *vr = DOWNCAST(sink);
    (void) ctx;
    (void) session;

    bool ok = sc_mutex_init(&vr->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&vr->queue_cond);
    if (!ok) {
        goto error_destroy_mutex;
    }

    ok = sc_cond_init(&vr->wait_cond);
    if (!ok) {
        goto error_destroy_queue_cond;
    }

    sc_clock_init(&vr->clock);
    sc_vecdeque_init(&vr->queue);
    vr->stopped = false;

    if (!sc_frame_source_sinks_open(&vr->frame_source, ctx, session)) {
        goto error_destroy_wait_cond;
    }

    ok = sc_thread_create(&vr->thread, run_buffering, "scrcpy-vruf", vr);
    if (!ok) {
        LOGE("Could not start buffering thread");
        goto error_close_sinks;
    }

    return true;

error_close_sinks:
    sc_frame_source_sinks_close(&vr->frame_source);
error_destroy_wait_cond:
    sc_cond_destroy(&vr->wait_cond);
error_destroy_queue_cond:
    sc_cond_destroy(&vr->queue_cond);
error_destroy_mutex:
    sc_mutex_destroy(&vr->mutex);

    return false;
}

static void
sc_video_regulator_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_video_regulator *vr = DOWNCAST(sink);

    sc_mutex_lock(&vr->mutex);
    vr->stopped = true;
    sc_cond_signal(&vr->queue_cond);
    sc_cond_signal(&vr->wait_cond);
    sc_mutex_unlock(&vr->mutex);

    sc_thread_join(&vr->thread, NULL);

    sc_frame_source_sinks_close(&vr->frame_source);

    sc_cond_destroy(&vr->wait_cond);
    sc_cond_destroy(&vr->queue_cond);
    sc_mutex_destroy(&vr->mutex);
}

static bool
sc_video_regulator_frame_sink_push(struct sc_frame_sink *sink,
                                const AVFrame *frame) {
    struct sc_video_regulator *vr = DOWNCAST(sink);

    sc_mutex_lock(&vr->mutex);

    if (vr->stopped) {
        sc_mutex_unlock(&vr->mutex);
        return false;
    }

    sc_tick pts = SC_TICK_FROM_US(frame->pts);
    sc_clock_update(&vr->clock, sc_tick_now(), pts);
    sc_cond_signal(&vr->wait_cond);

    if (vr->first_frame_asap && vr->clock.range == 1) {
        sc_mutex_unlock(&vr->mutex);
        return sc_frame_source_sinks_push(&vr->frame_source, frame);
    }

    struct sc_delayed_packet *dpacket =
        sc_vecdeque_push_uninitialized(&vr->queue);
    if (!dpacket) {
        sc_mutex_unlock(&vr->mutex);
        LOG_OOM();
        return false;
    }

    bool ok = sc_delayed_packet_init_frame(dpacket, frame);
    if (!ok) {
        sc_mutex_unlock(&vr->mutex);
        LOG_OOM();
        return false;
    }

#ifdef SC_BUFFERING_DEBUG
    dpacket->push_date = sc_tick_now();
#endif

    sc_cond_signal(&vr->queue_cond);

    sc_mutex_unlock(&vr->mutex);

    return true;
}

static bool
sc_video_regulator_frame_sink_push_session(struct sc_frame_sink *sink,
                                      const struct sc_stream_session *session) {
    struct sc_video_regulator *vr = DOWNCAST(sink);

    sc_mutex_lock(&vr->mutex);

    if (vr->stopped) {
        sc_mutex_unlock(&vr->mutex);
        return false;
    }

    struct sc_delayed_packet *dpacket =
        sc_vecdeque_push_uninitialized(&vr->queue);
    if (!dpacket) {
        sc_mutex_unlock(&vr->mutex);
        LOG_OOM();
        return false;
    }

    sc_delayed_packet_init_session(dpacket, session);

#ifdef SC_BUFFERING_DEBUG
    dpacket->push_date = sc_tick_now();
#endif

    sc_cond_signal(&vr->queue_cond);

    sc_mutex_unlock(&vr->mutex);

    return true;
}

void
sc_video_regulator_init(struct sc_video_regulator *vr, sc_tick delay,
                        bool first_frame_asap) {
    assert(delay > 0);

    vr->delay = delay;
    vr->first_frame_asap = first_frame_asap;

    sc_frame_source_init(&vr->frame_source);

    static const struct sc_frame_sink_ops ops = {
        .open = sc_video_regulator_frame_sink_open,
        .close = sc_video_regulator_frame_sink_close,
        .push = sc_video_regulator_frame_sink_push,
        .push_session = sc_video_regulator_frame_sink_push_session,
    };

    vr->frame_sink.ops = &ops;
}
