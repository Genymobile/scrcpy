#include "delay_buffer.h"

#include <assert.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>

#include "util/log.h"

/** Downcast frame_sink to sc_delay_buffer */
#define DOWNCAST(SINK) container_of(SINK, struct sc_delay_buffer, frame_sink)

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
    struct sc_delay_buffer *db = data;

    assert(db->delay > 0);

    for (;;) {
        sc_mutex_lock(&db->mutex);

        while (!db->stopped && sc_vecdeque_is_empty(&db->queue)) {
            sc_cond_wait(&db->queue_cond, &db->mutex);
        }

        if (db->stopped) {
            sc_mutex_unlock(&db->mutex);
            goto stopped;
        }

        struct sc_delayed_packet dpacket = sc_vecdeque_pop(&db->queue);

        bool ok;
        if (dpacket.type == SC_DELAYED_PACKET_TYPE_FRAME) {
            sc_tick max_deadline = sc_tick_now() + db->delay;
            // PTS (written by the server) are expressed in microseconds
            sc_tick pts = SC_TICK_FROM_US(dpacket.frame->pts);

            bool timed_out = false;
            while (!db->stopped && !timed_out) {
                sc_tick deadline = sc_clock_to_system_time(&db->clock, pts)
                                 + db->delay;
                if (deadline > max_deadline) {
                    deadline = max_deadline;
                }

                timed_out =
                    !sc_cond_timedwait(&db->wait_cond, &db->mutex, deadline);
            }

            bool stopped = db->stopped;
            sc_mutex_unlock(&db->mutex);

            if (stopped) {
                sc_delayed_packet_destroy(&dpacket);
                goto stopped;
            }

#ifdef SC_BUFFERING_DEBUG
            LOGD("Buffering: %" PRItick ";%" PRItick ";%" PRItick,
                 pts, dframe.push_date, sc_tick_now());
#endif

            ok = sc_frame_source_sinks_push(&db->frame_source, dpacket.frame);
        } else {
            assert(dpacket.type == SC_DELAYED_PACKET_TYPE_SESSION);
            sc_mutex_unlock(&db->mutex);
            ok = sc_frame_source_sinks_push_session(&db->frame_source,
                                                    &dpacket.session);
        }

        sc_delayed_packet_destroy(&dpacket);
        if (!ok) {
            LOGE("Delayed packet could not be pushed, stopping");
            sc_mutex_lock(&db->mutex);
            // Prevent to push any new packet
            db->stopped = true;
            sc_mutex_unlock(&db->mutex);
            goto stopped;
        }
    }

stopped:
    assert(db->stopped);

    // Flush queue
    while (!sc_vecdeque_is_empty(&db->queue)) {
        struct sc_delayed_packet *dpacket = sc_vecdeque_popref(&db->queue);
        sc_delayed_packet_destroy(dpacket);
    }

    LOGD("Buffering thread ended");

    return 0;
}

static bool
sc_delay_buffer_frame_sink_open(struct sc_frame_sink *sink,
                                const AVCodecContext *ctx,
                                const struct sc_stream_session *session) {
    struct sc_delay_buffer *db = DOWNCAST(sink);
    (void) ctx;
    (void) session;

    bool ok = sc_mutex_init(&db->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&db->queue_cond);
    if (!ok) {
        goto error_destroy_mutex;
    }

    ok = sc_cond_init(&db->wait_cond);
    if (!ok) {
        goto error_destroy_queue_cond;
    }

    sc_clock_init(&db->clock);
    sc_vecdeque_init(&db->queue);
    db->stopped = false;

    if (!sc_frame_source_sinks_open(&db->frame_source, ctx, session)) {
        goto error_destroy_wait_cond;
    }

    ok = sc_thread_create(&db->thread, run_buffering, "scrcpy-dbuf", db);
    if (!ok) {
        LOGE("Could not start buffering thread");
        goto error_close_sinks;
    }

    return true;

error_close_sinks:
    sc_frame_source_sinks_close(&db->frame_source);
error_destroy_wait_cond:
    sc_cond_destroy(&db->wait_cond);
error_destroy_queue_cond:
    sc_cond_destroy(&db->queue_cond);
error_destroy_mutex:
    sc_mutex_destroy(&db->mutex);

    return false;
}

static void
sc_delay_buffer_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_delay_buffer *db = DOWNCAST(sink);

    sc_mutex_lock(&db->mutex);
    db->stopped = true;
    sc_cond_signal(&db->queue_cond);
    sc_cond_signal(&db->wait_cond);
    sc_mutex_unlock(&db->mutex);

    sc_thread_join(&db->thread, NULL);

    sc_frame_source_sinks_close(&db->frame_source);

    sc_cond_destroy(&db->wait_cond);
    sc_cond_destroy(&db->queue_cond);
    sc_mutex_destroy(&db->mutex);
}

static bool
sc_delay_buffer_frame_sink_push(struct sc_frame_sink *sink,
                                const AVFrame *frame) {
    struct sc_delay_buffer *db = DOWNCAST(sink);

    sc_mutex_lock(&db->mutex);

    if (db->stopped) {
        sc_mutex_unlock(&db->mutex);
        return false;
    }

    sc_tick pts = SC_TICK_FROM_US(frame->pts);
    sc_clock_update(&db->clock, sc_tick_now(), pts);
    sc_cond_signal(&db->wait_cond);

    if (db->first_frame_asap && db->clock.range == 1) {
        sc_mutex_unlock(&db->mutex);
        return sc_frame_source_sinks_push(&db->frame_source, frame);
    }

    struct sc_delayed_packet *dpacket = sc_vecdeque_push_hole(&db->queue);
    if (!dpacket) {
        sc_mutex_unlock(&db->mutex);
        LOG_OOM();
        return false;
    }

    bool ok = sc_delayed_packet_init_frame(dpacket, frame);
    if (!ok) {
        sc_mutex_unlock(&db->mutex);
        LOG_OOM();
        return false;
    }

#ifdef SC_BUFFERING_DEBUG
    dpacket->push_date = sc_tick_now();
#endif

    sc_cond_signal(&db->queue_cond);

    sc_mutex_unlock(&db->mutex);

    return true;
}

static bool
sc_delay_buffer_frame_sink_push_session(struct sc_frame_sink *sink,
                                      const struct sc_stream_session *session) {
    struct sc_delay_buffer *db = DOWNCAST(sink);

    sc_mutex_lock(&db->mutex);

    if (db->stopped) {
        sc_mutex_unlock(&db->mutex);
        return false;
    }

    struct sc_delayed_packet *dpacket = sc_vecdeque_push_hole(&db->queue);
    if (!dpacket) {
        sc_mutex_unlock(&db->mutex);
        LOG_OOM();
        return false;
    }

    sc_delayed_packet_init_session(dpacket, session);

#ifdef SC_BUFFERING_DEBUG
    dpacket->push_date = sc_tick_now();
#endif

    sc_cond_signal(&db->queue_cond);

    sc_mutex_unlock(&db->mutex);

    return true;
}

void
sc_delay_buffer_init(struct sc_delay_buffer *db, sc_tick delay,
                     bool first_frame_asap) {
    assert(delay > 0);

    db->delay = delay;
    db->first_frame_asap = first_frame_asap;

    sc_frame_source_init(&db->frame_source);

    static const struct sc_frame_sink_ops ops = {
        .open = sc_delay_buffer_frame_sink_open,
        .close = sc_delay_buffer_frame_sink_close,
        .push = sc_delay_buffer_frame_sink_push,
        .push_session = sc_delay_buffer_frame_sink_push_session,
    };

    db->frame_sink.ops = &ops;
}
