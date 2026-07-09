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

    sc_tick now = sc_tick_now();
    if (!fk->first_frame_tick) {
        fk->first_frame_tick = now; // recording start reference (wall clock)
    }
    fk->last_frame_tick = now;
    fk->size.width = fk->latest->width;
    fk->size.height = fk->latest->height;
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
    fk->size.width = 0;
    fk->size.height = 0;
    fk->first_frame_tick = 0;
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

void
sc_frame_keeper_reset(struct sc_frame_keeper *fk) {
    sc_mutex_lock(&fk->mutex);
    av_frame_unref(fk->latest);
    fk->last_frame_tick = 0;
    fk->size.width = 0;
    fk->size.height = 0;
    fk->first_frame_tick = 0;
    sc_mutex_unlock(&fk->mutex);
}

bool
sc_frame_keeper_video_time_ms(struct sc_frame_keeper *fk, int64_t *out_ms) {
    sc_mutex_lock(&fk->mutex);
    bool ok = fk->first_frame_tick != 0;
    if (ok) {
        // Wall-clock elapsed since the first frame = the position in the
        // real-time mp4 playback timeline. (Frame PTS would freeze while the
        // screen is static, since the encoder only emits frames on change.)
        *out_ms = SC_TICK_TO_MS(sc_tick_now() - fk->first_frame_tick);
        if (*out_ms < 0) {
            *out_ms = 0;
        }
    }
    sc_mutex_unlock(&fk->mutex);
    return ok;
}

bool
sc_frame_keeper_wait_size(struct sc_frame_keeper *fk, sc_tick deadline,
                          struct sc_size *size) {
    bool ok = false;

    sc_mutex_lock(&fk->mutex);
    while (!fk->last_frame_tick) {
        if (!sc_cond_timedwait(&fk->cond, &fk->mutex, deadline)) {
            goto end; // timeout
        }
        if (!fk->opened && !fk->last_frame_tick) {
            goto end; // closed without a frame
        }
    }

    *size = fk->size;
    ok = true;

end:
    sc_mutex_unlock(&fk->mutex);
    return ok;
}
