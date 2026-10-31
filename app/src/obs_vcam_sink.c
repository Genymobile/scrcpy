#include "obs_vcam_sink.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

#include "util/log.h"
#include "util/tick.h"

/** Downcast frame_sink to sc_obs_vcam_sink */
#define DOWNCAST(SINK) container_of(SINK, struct sc_obs_vcam_sink, frame_sink)

// Shared memory queue read by the OBS Virtual Camera filter, see
// <https://github.com/obsproject/obs-studio/blob/master/plugins/win-dshow/shared-memory-queue.c>
#define SC_OBS_VCAM_NAME L"OBSVirtualCamVideo"
#define SC_OBS_VCAM_SLOTS 3
#define SC_OBS_VCAM_FRAME_HEADER_SIZE 32 // contains a 64-bit timestamp
#define SC_OBS_VCAM_INTERVAL (10000000 / 30) // 30 fps, in 100ns units

// The filter stops reading after 10 consecutive reads of the same frame, so
// the last frame must be written again while the device screen is static
#define SC_OBS_VCAM_REPEAT_DELAY SC_TICK_FROM_MS(100)

enum sc_obs_vcam_state {
    SC_OBS_VCAM_STATE_INVALID,
    SC_OBS_VCAM_STATE_STARTING,
    SC_OBS_VCAM_STATE_READY,
    SC_OBS_VCAM_STATE_STOPPING,
};

struct sc_obs_vcam_header {
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    volatile uint32_t state;
    uint32_t offsets[SC_OBS_VCAM_SLOTS];
    uint32_t type;
    // The filter reads the frame size on every frame, so it may change
    volatile uint32_t cx;
    volatile uint32_t cy;
    uint64_t interval;
    uint32_t reserved[8];
};

static inline uint32_t
align32(uint32_t size) {
    return (size + 31) & ~UINT32_C(31);
}

static bool
write_frame(struct sc_obs_vcam_sink *vs, const AVFrame *frame) {
    assert(frame->format == AV_PIX_FMT_YUV420P);

    uint32_t w = frame->width;
    uint32_t h = frame->height;
    if (w * h * 3 / 2 > vs->capacity) {
        LOGE("OBS virtual camera: frame size %" PRIu32 "x%" PRIu32
             " exceeds the initial size", w, h);
        return false;
    }

    struct sc_obs_vcam_header *header = vs->header;
    uint32_t inc = header->write_idx + 1;
    header->write_idx = inc;

    uint8_t *slot = (uint8_t *) header + header->offsets[inc % SC_OBS_VCAM_SLOTS];
    // In 100ns units, like DirectShow timestamps
    *(uint64_t *) slot = (uint64_t) sc_tick_now() * 10;
    uint8_t *dst = slot + SC_OBS_VCAM_FRAME_HEADER_SIZE;

    // Convert from I420 to NV12, with no padding
    for (uint32_t y = 0; y < h; ++y) {
        memcpy(dst, frame->data[0] + y * frame->linesize[0], w);
        dst += w;
    }
    for (uint32_t y = 0; y < h / 2; ++y) {
        const uint8_t *u = frame->data[1] + y * frame->linesize[1];
        const uint8_t *v = frame->data[2] + y * frame->linesize[2];
        for (uint32_t x = 0; x < w / 2; ++x) {
            *dst++ = u[x];
            *dst++ = v[x];
        }
    }

    header->cx = w;
    header->cy = h;
    // Publish the frame only once fully written
    MemoryBarrier();
    header->read_idx = inc;
    header->state = SC_OBS_VCAM_STATE_READY;

    return true;
}

static int
run_obs_vcam_sink(void *data) {
    struct sc_obs_vcam_sink *vs = data;

    for (;;) {
        sc_mutex_lock(&vs->mutex);

        sc_tick deadline = sc_tick_now() + SC_OBS_VCAM_REPEAT_DELAY;
        bool timed_out = false;
        while (!vs->stopped && !vs->has_frame && !timed_out) {
            timed_out = !sc_cond_timedwait(&vs->cond, &vs->mutex, deadline);
        }

        if (vs->stopped) {
            sc_mutex_unlock(&vs->mutex);
            break;
        }

        if (vs->has_frame) {
            vs->has_frame = false;
            av_frame_unref(vs->frame);
            sc_frame_buffer_consume(&vs->fb, vs->frame);
        }
        // Otherwise, write the last frame again
        sc_mutex_unlock(&vs->mutex);

        if (!vs->frame->buf[0]) {
            // No frame received yet
            continue;
        }

        bool ok = write_frame(vs, vs->frame);
        if (!ok) {
            LOGE("Could not send frame to OBS virtual camera");
            break;
        }
    }

    LOGD("OBS virtual camera thread ended");

    return 0;
}

static bool
create_mapping(struct sc_obs_vcam_sink *vs, uint32_t width, uint32_t height) {
    // Rotating the device swaps width and height, so the frame data size
    // remains the same
    vs->capacity = width * height * 3 / 2;

    uint32_t header_size = align32(sizeof(struct sc_obs_vcam_header));
    uint32_t slot_size =
        align32(SC_OBS_VCAM_FRAME_HEADER_SIZE + vs->capacity);
    uint32_t size = header_size + SC_OBS_VCAM_SLOTS * slot_size;

    HANDLE existing =
        OpenFileMappingW(FILE_MAP_READ, FALSE, SC_OBS_VCAM_NAME);
    if (existing) {
        CloseHandle(existing);
        LOGE("OBS virtual camera is already in use (by OBS or another scrcpy "
             "instance)");
        return false;
    }

    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
                                        PAGE_READWRITE, 0, size,
                                        SC_OBS_VCAM_NAME);
    if (!mapping) {
        sc_log_windows_error("Could not create OBS virtual camera memory",
                             GetLastError());
        return false;
    }

    struct sc_obs_vcam_header *header =
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!header) {
        sc_log_windows_error("Could not map OBS virtual camera memory",
                             GetLastError());
        CloseHandle(mapping);
        return false;
    }

    // The mapping is zero-initialized
    for (unsigned i = 0; i < SC_OBS_VCAM_SLOTS; ++i) {
        header->offsets[i] = header_size + i * slot_size;
    }
    header->cx = width;
    header->cy = height;
    header->interval = SC_OBS_VCAM_INTERVAL;
    header->state = SC_OBS_VCAM_STATE_STARTING;

    vs->mapping = mapping;
    vs->header = header;
    return true;
}

static void
destroy_mapping(struct sc_obs_vcam_sink *vs) {
    vs->header->state = SC_OBS_VCAM_STATE_STOPPING;
    UnmapViewOfFile(vs->header);
    CloseHandle(vs->mapping);
}

static bool
sc_obs_vcam_sink_open(struct sc_obs_vcam_sink *vs, const AVCodecContext *ctx,
                      const struct sc_stream_session *session) {
    assert(ctx->pix_fmt == AV_PIX_FMT_YUV420P);
    (void) session;

    bool ok = sc_frame_buffer_init(&vs->fb);
    if (!ok) {
        return false;
    }

    ok = sc_mutex_init(&vs->mutex);
    if (!ok) {
        goto error_frame_buffer_destroy;
    }

    ok = sc_cond_init(&vs->cond);
    if (!ok) {
        goto error_mutex_destroy;
    }

    vs->frame = av_frame_alloc();
    if (!vs->frame) {
        LOG_OOM();
        goto error_cond_destroy;
    }

    ok = create_mapping(vs, ctx->width, ctx->height);
    if (!ok) {
        goto error_av_frame_free;
    }

    vs->has_frame = false;
    vs->stopped = false;

    LOGD("Starting OBS virtual camera thread");
    ok = sc_thread_create(&vs->thread, run_obs_vcam_sink, "scrcpy-obs-vcam",
                          vs);
    if (!ok) {
        LOGE("Could not start OBS virtual camera thread");
        goto error_destroy_mapping;
    }

    LOGI("OBS virtual camera sink started");

    return true;

error_destroy_mapping:
    destroy_mapping(vs);
error_av_frame_free:
    av_frame_free(&vs->frame);
error_cond_destroy:
    sc_cond_destroy(&vs->cond);
error_mutex_destroy:
    sc_mutex_destroy(&vs->mutex);
error_frame_buffer_destroy:
    sc_frame_buffer_destroy(&vs->fb);

    return false;
}

static void
sc_obs_vcam_sink_close(struct sc_obs_vcam_sink *vs) {
    sc_mutex_lock(&vs->mutex);
    vs->stopped = true;
    sc_cond_signal(&vs->cond);
    sc_mutex_unlock(&vs->mutex);

    sc_thread_join(&vs->thread, NULL);

    destroy_mapping(vs);
    av_frame_free(&vs->frame);
    sc_cond_destroy(&vs->cond);
    sc_mutex_destroy(&vs->mutex);
    sc_frame_buffer_destroy(&vs->fb);
}

static enum sc_sink_result
sc_obs_vcam_sink_push(struct sc_obs_vcam_sink *vs, const AVFrame *frame) {
    sc_mutex_lock(&vs->mutex);

    if (vs->stopped) {
        sc_mutex_unlock(&vs->mutex);
        return SC_SINK_STOPPED;
    }

    bool previous_skipped = sc_frame_buffer_has_frame(&vs->fb);
    bool ok = sc_frame_buffer_push(&vs->fb, frame);
    if (!ok) {
        sc_mutex_unlock(&vs->mutex);
        return SC_SINK_KO;
    }

    if (!previous_skipped) {
        vs->has_frame = true;
        sc_cond_signal(&vs->cond);
    }

    sc_mutex_unlock(&vs->mutex);
    return SC_SINK_OK;
}

static bool
sc_obs_vcam_frame_sink_open(struct sc_frame_sink *sink,
                            const AVCodecContext *ctx,
                            const struct sc_stream_session *session) {
    struct sc_obs_vcam_sink *vs = DOWNCAST(sink);
    return sc_obs_vcam_sink_open(vs, ctx, session);
}

static void
sc_obs_vcam_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_obs_vcam_sink *vs = DOWNCAST(sink);
    sc_obs_vcam_sink_close(vs);
}

static enum sc_sink_result
sc_obs_vcam_frame_sink_push(struct sc_frame_sink *sink, const AVFrame *frame) {
    struct sc_obs_vcam_sink *vs = DOWNCAST(sink);
    return sc_obs_vcam_sink_push(vs, frame);
}

void
sc_obs_vcam_sink_init(struct sc_obs_vcam_sink *vs) {
    static const struct sc_frame_sink_ops ops = {
        .open = sc_obs_vcam_frame_sink_open,
        .close = sc_obs_vcam_frame_sink_close,
        .push = sc_obs_vcam_frame_sink_push,
    };

    vs->frame_sink.ops = &ops;
}
