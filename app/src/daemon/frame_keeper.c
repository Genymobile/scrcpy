#include "frame_keeper.h"

#include <assert.h>

#include "util/log.h"

/** Downcast frame_sink to sc_frame_keeper */
#define DOWNCAST(SINK) container_of(SINK, struct sc_frame_keeper, frame_sink)

static bool
sc_frame_keeper_frame_sink_open(struct sc_frame_sink *sink,
                                const AVCodecContext *ctx,
                                const struct sc_stream_session *session) {
    (void) ctx;
    (void) session;
    struct sc_frame_keeper *fk = DOWNCAST(sink);

    sc_mutex_lock(&fk->mutex);
    fk->opened = true;
    sc_cond_broadcast(&fk->cond);
    sc_mutex_unlock(&fk->mutex);

    return true;
}

static void
sc_frame_keeper_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_frame_keeper *fk = DOWNCAST(sink);

    sc_mutex_lock(&fk->mutex);
    fk->opened = false;
    sc_cond_broadcast(&fk->cond);
    sc_mutex_unlock(&fk->mutex);
}

static bool
sc_frame_keeper_frame_sink_push(struct sc_frame_sink *sink,
                                const AVFrame *frame) {
    struct sc_frame_keeper *fk = DOWNCAST(sink);

    sc_mutex_lock(&fk->mutex);

    // Use a temporary frame so that `latest` is preserved on error
    int r = av_frame_ref(fk->tmp, frame);
    if (r) {
        sc_mutex_unlock(&fk->mutex);
        LOGE("Frame keeper: could not ref frame: %d", r);
        // Do not fail the whole pipeline for one missed frame
        return true;
    }

    AVFrame *swap = fk->latest;
    fk->latest = fk->tmp;
    fk->tmp = swap;
    av_frame_unref(fk->tmp);

    fk->last_frame_tick = sc_tick_now();
    sc_cond_broadcast(&fk->cond);

    sc_mutex_unlock(&fk->mutex);

    return true;
}

bool
sc_frame_keeper_init(struct sc_frame_keeper *fk) {
    fk->latest = NULL;
    fk->tmp = av_frame_alloc();
    if (!fk->tmp) {
        LOG_OOM();
        return false;
    }

    // `latest` stays NULL until the first push; allocate lazily via swap:
    // both frames must exist for the swap, so allocate `latest` too
    fk->latest = av_frame_alloc();
    if (!fk->latest) {
        LOG_OOM();
        av_frame_free(&fk->tmp);
        return false;
    }

    bool ok = sc_mutex_init(&fk->mutex);
    if (!ok) {
        av_frame_free(&fk->latest);
        av_frame_free(&fk->tmp);
        return false;
    }

    ok = sc_cond_init(&fk->cond);
    if (!ok) {
        sc_mutex_destroy(&fk->mutex);
        av_frame_free(&fk->latest);
        av_frame_free(&fk->tmp);
        return false;
    }

    fk->last_frame_tick = 0;
    fk->opened = false;

    static const struct sc_frame_sink_ops ops = {
        .open = sc_frame_keeper_frame_sink_open,
        .close = sc_frame_keeper_frame_sink_close,
        .push = sc_frame_keeper_frame_sink_push,
    };

    fk->frame_sink.ops = &ops;

    return true;
}

void
sc_frame_keeper_destroy(struct sc_frame_keeper *fk) {
    sc_cond_destroy(&fk->cond);
    sc_mutex_destroy(&fk->mutex);
    av_frame_free(&fk->latest);
    av_frame_free(&fk->tmp);
}

bool
sc_frame_keeper_get_since(struct sc_frame_keeper *fk, AVFrame *dst,
                          sc_tick min_tick, sc_tick deadline,
                          sc_tick *out_age) {
    bool ok = false;

    sc_mutex_lock(&fk->mutex);

    while (fk->last_frame_tick <= min_tick) {
        if (!sc_cond_timedwait(&fk->cond, &fk->mutex, deadline)) {
            goto end; // timeout
        }
        if (!fk->opened && fk->last_frame_tick <= min_tick) {
            goto end; // closed without a (new) frame
        }
    }

    assert(fk->latest);
    if (av_frame_ref(dst, fk->latest)) {
        LOGE("Frame keeper: could not ref frame for reader");
        goto end;
    }

    if (out_age) {
        *out_age = sc_tick_now() - fk->last_frame_tick;
    }
    ok = true;

end:
    sc_mutex_unlock(&fk->mutex);
    return ok;
}

bool
sc_frame_keeper_get(struct sc_frame_keeper *fk, AVFrame *dst,
                    sc_tick deadline, sc_tick *out_age) {
    // 0 means "no frame yet" for last_frame_tick, so any frame matches
    return sc_frame_keeper_get_since(fk, dst, 0, deadline, out_age);
}

sc_tick
sc_frame_keeper_last_tick(struct sc_frame_keeper *fk) {
    sc_mutex_lock(&fk->mutex);
    sc_tick tick = fk->last_frame_tick;
    sc_mutex_unlock(&fk->mutex);
    return tick;
}
