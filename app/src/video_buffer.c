#include "video_buffer.h"

#include <assert.h>
#include <stdlib.h>

#include <libavutil/avutil.h>
#include <libavformat/avformat.h>

#include "util/log.h"

static bool
sc_delay_buffer_on_new_frame(struct sc_delay_buffer *db, const AVFrame *frame,
                             void *userdata) {
    (void) db;

    struct sc_video_buffer *vb = userdata;

    bool previous_skipped;
    bool ok = sc_frame_buffer_push(&vb->fb, frame, &previous_skipped);
    if (!ok) {
        return false;
    }

    return vb->cbs->on_new_frame(vb, previous_skipped, vb->cbs_userdata);
}

bool
sc_video_buffer_init(struct sc_video_buffer *vb, sc_tick delay,
                     const struct sc_video_buffer_callbacks *cbs,
                     void *cbs_userdata) {
    bool ok = sc_frame_buffer_init(&vb->fb);
    if (!ok) {
        return false;
    }

    static const struct sc_delay_buffer_callbacks db_cbs = {
        .on_new_frame = sc_delay_buffer_on_new_frame,
    };

    ok = sc_delay_buffer_init(&vb->db, delay, &db_cbs, vb);
    if (!ok) {
        sc_frame_buffer_destroy(&vb->fb);
        return false;
    }

    assert(cbs);
    assert(cbs->on_new_frame);

    vb->cbs = cbs;
    vb->cbs_userdata = cbs_userdata;

    return true;
}

bool
sc_video_buffer_start(struct sc_video_buffer *vb) {
    return sc_delay_buffer_start(&vb->db);
}

void
sc_video_buffer_stop(struct sc_video_buffer *vb) {
    return sc_delay_buffer_stop(&vb->db);
}

void
sc_video_buffer_join(struct sc_video_buffer *vb) {
    return sc_delay_buffer_join(&vb->db);
}

void
sc_video_buffer_destroy(struct sc_video_buffer *vb) {
    sc_frame_buffer_destroy(&vb->fb);
    sc_delay_buffer_destroy(&vb->db);
}

bool
sc_video_buffer_push(struct sc_video_buffer *vb, const AVFrame *frame) {
    return sc_delay_buffer_push(&vb->db, frame);
}

void
sc_video_buffer_consume(struct sc_video_buffer *vb, AVFrame *dst) {
    sc_frame_buffer_consume(&vb->fb, dst);
}
