#include "clip_buffer.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>

#include "compat.h"
#include "daemon/frame_keeper.h"
#include "daemon/protocol.h"
#include "util/log.h"

/** Downcast packet_sink to sc_clip_buffer */
#define DOWNCAST(SINK) container_of(SINK, struct sc_clip_buffer, packet_sink)

static const AVRational SCRCPY_TIME_BASE = {1, 1000000}; // timestamps in us

static const struct sc_clip_format SC_CLIP_FORMAT_MP4 = {
    .muxer_name = "mp4",
    .container = "mp4",
    .extension = ".mp4",
    .mime_type = "video/mp4",
};

static const struct sc_clip_format SC_CLIP_FORMAT_WEBM = {
    .muxer_name = "webm",
    .container = "webm",
    .extension = ".webm",
    .mime_type = "video/webm",
};

const struct sc_clip_format *
sc_clip_format_for_codec(enum AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_VP8:
            // FFmpeg/upstream scrcpy do not support muxing VP8 into MP4.
            return &SC_CLIP_FORMAT_WEBM;
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_HEVC:
#ifdef SCRCPY_LAVC_HAS_AV1
        case AV_CODEC_ID_AV1:
#endif
        case AV_CODEC_ID_VP9:
            return &SC_CLIP_FORMAT_MP4;
        default:
            return NULL;
    }
}

// Create an unlinked temporary spool file and return its fd (-1 on error).
// Unlinking immediately means the file needs no cleanup path: the space is
// reclaimed by the OS when the fd is closed (or the daemon dies).
static int
create_spool(void) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) {
        dir = "/tmp";
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/scrcpy-auto-clip-XXXXXX", dir);
    int fd = mkstemp(path);
    if (fd == -1) {
        LOGW("Clip buffer: could not create spool file in %s", dir);
        return -1;
    }
    unlink(path);
    return fd;
}

static void
reset_locked(struct sc_clip_buffer *cb) {
    if (cb->fd != -1) {
        close(cb->fd);
        cb->fd = -1;
    }
    cb->spool_size = 0;
    for (size_t i = 0; i < cb->epoch_count; ++i) {
        struct sc_clip_epoch *epoch = &cb->epochs[i];
        avcodec_parameters_free(&epoch->par);
        free(epoch->config);
    }
    free(cb->epochs);
    cb->epochs = NULL;
    cb->epoch_count = 0;
    cb->epoch_cap = 0;
    cb->current_epoch = 0;
    free(cb->entries);
    cb->entries = NULL;
    cb->count = 0;
    cb->cap = 0;
    cb->failed = false;
}

static void
disable_locked(struct sc_clip_buffer *cb, const char *reason) {
    LOGW("Clip buffer: %s; clips disabled", reason);
    cb->opened = false;
    cb->failed = true;
    if (cb->fd != -1) {
        close(cb->fd);
        cb->fd = -1;
    }
}

static bool
sc_clip_buffer_packet_sink_open(struct sc_packet_sink *sink,
                                AVCodecContext *ctx,
                                const struct sc_stream_session *session);
static void
sc_clip_buffer_packet_sink_close(struct sc_packet_sink *sink);
static bool
sc_clip_buffer_packet_sink_push(struct sc_packet_sink *sink,
                                const AVPacket *packet);
static bool
sc_clip_buffer_packet_sink_push_session(
        struct sc_packet_sink *sink,
        const struct sc_stream_session *session);

bool
sc_clip_buffer_init(struct sc_clip_buffer *cb,
                    struct sc_frame_keeper *timeline) {
    memset(cb, 0, sizeof(*cb));
    cb->fd = -1;
    cb->timeline = timeline;

    static const struct sc_packet_sink_ops ops = {
        .open = sc_clip_buffer_packet_sink_open,
        .close = sc_clip_buffer_packet_sink_close,
        .push = sc_clip_buffer_packet_sink_push,
        .push_session = sc_clip_buffer_packet_sink_push_session,
    };
    cb->packet_sink.ops = &ops;

    return sc_mutex_init(&cb->mutex);
}

void
sc_clip_buffer_destroy(struct sc_clip_buffer *cb) {
    reset_locked(cb);
    sc_mutex_destroy(&cb->mutex);
}

bool
sc_clip_buffer_source_time_ms(struct sc_clip_buffer *cb, int64_t *out_ms) {
    int64_t origin;
    if (!sc_frame_keeper_get_timeline_anchor(cb->timeline, NULL, &origin)) {
        return false;
    }

    sc_mutex_lock(&cb->mutex);
    bool ok = cb->count != 0
           && cb->entries[cb->count - 1].pts >= origin;
    if (ok) {
        int64_t last = cb->entries[cb->count - 1].pts;
        *out_ms = (last - origin) / 1000;
    }
    sc_mutex_unlock(&cb->mutex);
    return ok;
}

bool
sc_clip_buffer_timeline_time_ms(struct sc_clip_buffer *cb, int64_t *out_ms) {
    return sc_frame_keeper_video_time_ms(cb->timeline, out_ms);
}

// ---- packet sink trait ------------------------------------------------------

static struct sc_clip_epoch *
append_epoch_locked(struct sc_clip_buffer *cb,
                    const AVCodecParameters *source) {
    AVCodecParameters *par = avcodec_parameters_alloc();
    if (!par) {
        LOG_OOM();
        return NULL;
    }
    if (avcodec_parameters_copy(par, source) < 0) {
        avcodec_parameters_free(&par);
        return NULL;
    }

    if (cb->epoch_count == cb->epoch_cap) {
        size_t cap = cb->epoch_cap ? cb->epoch_cap * 2 : 4;
        struct sc_clip_epoch *epochs =
            realloc(cb->epochs, cap * sizeof(*epochs));
        if (!epochs) {
            LOG_OOM();
            avcodec_parameters_free(&par);
            return NULL;
        }
        cb->epochs = epochs;
        cb->epoch_cap = cap;
    }

    struct sc_clip_epoch *epoch = &cb->epochs[cb->epoch_count];
    *epoch = (struct sc_clip_epoch) {
        .par = par,
        .config = NULL,
        .config_size = 0,
        .first_entry = cb->count,
    };
    cb->current_epoch = (uint32_t) cb->epoch_count;
    ++cb->epoch_count;
    return epoch;
}

static bool
sc_clip_buffer_packet_sink_open(struct sc_packet_sink *sink,
                                AVCodecContext *ctx,
                                const struct sc_stream_session *session) {
    (void) session;
    struct sc_clip_buffer *cb = DOWNCAST(sink);

    sc_mutex_lock(&cb->mutex);
    // A new stream session (e.g. device reconnect) restarts the buffer: PTS
    // epochs are not comparable across sessions
    reset_locked(cb);
    AVCodecParameters *initial = avcodec_parameters_alloc();
    if (initial && avcodec_parameters_from_context(initial, ctx) < 0) {
        avcodec_parameters_free(&initial);
    }
    bool epoch_ok = initial && append_epoch_locked(cb, initial);
    avcodec_parameters_free(&initial);
    cb->fd = create_spool();
    cb->opened = epoch_ok && cb->fd != -1;
    if (!cb->opened) {
        LOGW("Clip buffer: unavailable for this session");
        reset_locked(cb);
        cb->failed = true;
    }
    sc_mutex_unlock(&cb->mutex);
    // Never fail the demuxer pipeline: without a spool, clip requests will
    // simply return an error
    return true;
}

static bool
sc_clip_buffer_packet_sink_push_session(
        struct sc_packet_sink *sink,
        const struct sc_stream_session *session) {
    struct sc_clip_buffer *cb = DOWNCAST(sink);

    sc_mutex_lock(&cb->mutex);
    if (!cb->opened || !cb->epoch_count) {
        sc_mutex_unlock(&cb->mutex);
        return true;
    }

    const struct sc_clip_epoch *previous = &cb->epochs[cb->current_epoch];
    struct sc_clip_epoch *epoch = append_epoch_locked(cb, previous->par);
    if (epoch) {
        epoch->par->width = session->video.width;
        epoch->par->height = session->video.height;
    } else {
        disable_locked(cb, "could not snapshot a new stream session");
    }
    sc_mutex_unlock(&cb->mutex);
    // Clip buffering remains best-effort and must not fail the live stream.
    return true;
}

static void
sc_clip_buffer_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_clip_buffer *cb = DOWNCAST(sink);
    sc_mutex_lock(&cb->mutex);
    // Keep the spool: already-recorded ranges stay clippable until the next
    // session (or daemon exit)
    cb->opened = false;
    sc_mutex_unlock(&cb->mutex);
}

static bool
sc_clip_buffer_packet_sink_push(struct sc_packet_sink *sink,
                                const AVPacket *packet) {
    struct sc_clip_buffer *cb = DOWNCAST(sink);

    sc_mutex_lock(&cb->mutex);
    if (!cb->opened || cb->fd == -1) {
        goto out; // spool unavailable: drop silently, never block the stream
    }

    if (packet->pts == AV_NOPTS_VALUE) {
        // Config packet (codec extradata); keep the latest one
        uint8_t *config = packet->size ? malloc(packet->size) : NULL;
        if (!packet->size || config) {
            if (packet->size) {
                memcpy(config, packet->data, packet->size);
            }
            struct sc_clip_epoch *epoch = &cb->epochs[cb->current_epoch];
            free(epoch->config);
            epoch->config = config;
            epoch->config_size = packet->size;
        } else {
            disable_locked(cb, "could not retain codec configuration");
        }
        goto out;
    }

    if (cb->count == cb->cap) {
        size_t cap = cb->cap ? cb->cap * 2 : 1024;
        struct sc_clip_entry *entries =
            realloc(cb->entries, cap * sizeof(*entries));
        if (!entries) {
            disable_locked(cb, "could not grow packet index");
            goto out;
        }
        cb->entries = entries;
        cb->cap = cap;
    }

    ssize_t w = pwrite(cb->fd, packet->data, packet->size,
                       (off_t) cb->spool_size);
    if (w != packet->size) {
        // Disk full or I/O error: disable the buffer, keep the session alive
        disable_locked(cb, "spool write failed");
        goto out;
    }

    struct sc_clip_entry *e = &cb->entries[cb->count++];
    e->pts = packet->pts;
    e->offset = cb->spool_size;
    e->size = packet->size;
    e->epoch = cb->current_epoch;
    e->key = packet->flags & AV_PKT_FLAG_KEY;
    cb->spool_size += packet->size;

out:
    sc_mutex_unlock(&cb->mutex);
    return true;
}

// ---- selection --------------------------------------------------------------

// Binary search: index of the last entry with pts <= t, or -1 if none.
static ssize_t
last_at_or_before(const struct sc_clip_entry *entries, size_t count,
                  int64_t t) {
    ssize_t lo = 0, hi = (ssize_t) count - 1, res = -1;
    while (lo <= hi) {
        ssize_t mid = lo + (hi - lo) / 2;
        if (entries[mid].pts <= t) {
            res = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return res;
}

// Binary search: index of the last entry with pts < t, or -1 if none.
static ssize_t
last_before(const struct sc_clip_entry *entries, size_t count, int64_t t) {
    ssize_t lo = 0, hi = (ssize_t) count - 1, res = -1;
    while (lo <= hi) {
        ssize_t mid = lo + (hi - lo) / 2;
        if (entries[mid].pts < t) {
            res = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return res;
}

static int
find_timeline_origin(const struct sc_clip_entry *entries, size_t count,
                     int64_t origin, size_t *out_index) {
    size_t index = 0;
    while (index < count && entries[index].pts < origin) {
        ++index;
    }
    if (index == count) {
        return SC_CLIP_ERANGE;
    }
    if (entries[index].pts != origin || !entries[index].key) {
        return SC_CLIP_EINTERNAL;
    }
    *out_index = index;
    return 0;
}

bool
sc_clip_select(const struct sc_clip_entry *entries, size_t count,
               int64_t start_us, int64_t end_us, size_t *begin, size_t *stop) {
    if (!count || start_us >= end_us) {
        return false;
    }

    // `end_us` is an exclusive playback boundary. A packet stamped exactly
    // at the boundary belongs to the following segment.
    ssize_t last = last_before(entries, count, end_us);
    if (last < 0) {
        return false; // the window ends before the first packet
    }

    // Anchor at the entry at/before start (or the very first entry), then
    // walk back to its keyframe; a decoder cannot start mid-GOP
    ssize_t anchor = last_at_or_before(entries, count, start_us);
    if (anchor < 0) {
        anchor = 0;
    }
    while (anchor > 0 && !entries[anchor].key) {
        anchor--;
    }
    if (!entries[anchor].key) {
        // No keyframe at or before start: take the first keyframe inside the
        // window instead (streams normally start with one, so this is rare)
        while (anchor <= last && !entries[anchor].key) {
            anchor++;
        }
        if (anchor > last) {
            return false;
        }
    }

    *begin = (size_t) anchor;
    *stop = (size_t) last;
    return true;
}

int
sc_clip_select_epoch(const struct sc_clip_entry *entries, size_t count,
                     int64_t start_us, int64_t end_us, size_t *begin,
                     size_t *stop, int64_t *boundary_us) {
    if (!count || start_us >= end_us) {
        return SC_CLIP_ERANGE;
    }

    ssize_t last = last_before(entries, count, end_us);
    if (last < 0) {
        return SC_CLIP_ERANGE;
    }

    uint32_t epoch = entries[last].epoch;
    size_t epoch_first = (size_t) last;
    while (epoch_first && entries[epoch_first - 1].epoch == epoch) {
        --epoch_first;
    }

    // One muxed stream cannot change codec parameters halfway through. Both
    // sides remain independently extractable with their original timestamps.
    if (epoch_first && start_us < entries[epoch_first].pts
            // Public clip coordinates are integer milliseconds. Permit the
            // caller to restart at the millisecond bucket containing a
            // sub-millisecond epoch boundary; selection still snaps forward
            // to that epoch's first keyframe, so no packet crosses sessions.
            && entries[epoch_first].pts - start_us >= 1000) {
        if (boundary_us) {
            *boundary_us = entries[epoch_first].pts;
        }
        return SC_CLIP_ESESSION;
    }

    size_t local_begin;
    size_t local_stop;
    if (!sc_clip_select(&entries[epoch_first],
                        (size_t) last - epoch_first + 1,
                        start_us, end_us, &local_begin, &local_stop)) {
        return SC_CLIP_ERANGE;
    }

    *begin = epoch_first + local_begin;
    *stop = epoch_first + local_stop;
    return 0;
}

static bool
buffer_equal(const uint8_t *a, size_t a_size, const uint8_t *b,
             size_t b_size) {
    return a_size == b_size
        && (!a_size || !memcmp(a, b, a_size));
}

static bool
codec_parameters_compatible(const AVCodecParameters *a,
                            const AVCodecParameters *b) {
    // Compare every codec/geometry/color field which describes how samples in
    // one muxed video track must be interpreted. Bit rate and padding are not
    // decoder configuration and may legitimately vary across an encoder
    // restart.
    return a && b
        && a->codec_type == b->codec_type
        && a->codec_id == b->codec_id
        && a->codec_tag == b->codec_tag
        && a->format == b->format
        && a->profile == b->profile
        && a->level == b->level
        && a->width == b->width
        && a->height == b->height
        && a->sample_aspect_ratio.num == b->sample_aspect_ratio.num
        && a->sample_aspect_ratio.den == b->sample_aspect_ratio.den
        && a->field_order == b->field_order
        && a->color_range == b->color_range
        && a->color_primaries == b->color_primaries
        && a->color_trc == b->color_trc
        && a->color_space == b->color_space
        && a->chroma_location == b->chroma_location
        && buffer_equal(a->extradata, a->extradata_size,
                        b->extradata, b->extradata_size);
}

static bool
epochs_compatible(const struct sc_clip_epoch *a,
                  const struct sc_clip_epoch *b) {
    return codec_parameters_compatible(a->par, b->par)
        && buffer_equal(a->config, a->config_size,
                        b->config, b->config_size);
}

// ---- extraction -------------------------------------------------------------

static int64_t
packet_duration_us(const struct sc_clip_entry *entries, size_t count,
                   size_t index, int64_t end_us) {
    assert(count && index < count);
    int64_t packet_end = index + 1 < count ? entries[index + 1].pts : end_us;
    return packet_end - entries[index].pts;
}

#ifdef SC_TEST
int64_t
sc_clip_packet_duration_us(const struct sc_clip_entry *entries, size_t count,
                           size_t index, int64_t end_us) {
    return packet_duration_us(entries, count, index, end_us);
}

int
sc_clip_find_timeline_origin(const struct sc_clip_entry *entries, size_t count,
                             int64_t origin, size_t *out_index) {
    return find_timeline_origin(entries, count, origin, out_index);
}

bool
sc_clip_epochs_compatible(const struct sc_clip_epoch *a,
                          const struct sc_clip_epoch *b) {
    return epochs_compatible(a, b);
}
#endif

static int
mux_entries(int spool_fd, const AVCodecParameters *par,
            const struct sc_clip_entry *entries, size_t count,
            const uint8_t *config, size_t config_size, int64_t end_us,
            const struct sc_clip_format *format, uint8_t **out,
            size_t *out_size) {
    int ret = SC_CLIP_EINTERNAL;
    AVFormatContext *ctx = NULL;
    AVPacket *packet = NULL;
    bool header_written = false;

    if (avformat_alloc_output_context2(&ctx, NULL, format->muxer_name,
                                       NULL) < 0) {
        return SC_CLIP_EINTERNAL;
    }

    AVStream *stream = avformat_new_stream(ctx, NULL);
    if (!stream || avcodec_parameters_copy(stream->codecpar, par) < 0) {
        goto end;
    }
    if (config_size) { // codec extradata comes from the config packet
        uint8_t *extradata = av_malloc(config_size
                                       + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!extradata) {
            goto end;
        }
        memcpy(extradata, config, config_size);
        memset(extradata + config_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
        av_free(stream->codecpar->extradata);
        stream->codecpar->extradata = extradata;
        stream->codecpar->extradata_size = config_size;
    }

    if (avio_open_dyn_buf(&ctx->pb) < 0) {
        goto end;
    }
    if (avformat_write_header(ctx, NULL) < 0) {
        goto end;
    }
    if (avio_tell(ctx->pb) > (int64_t) SC_DAEMON_MAX_BINARY_PAYLOAD) {
        ret = SC_CLIP_ETOOLARGE;
        goto end;
    }
    header_written = true;

    packet = av_packet_alloc();
    if (!packet) {
        goto end;
    }

    int64_t base = entries[0].pts; // the clip starts at t = 0
    for (size_t i = 0; i < count; ++i) {
        const struct sc_clip_entry *e = &entries[i];
        uint8_t *data = av_malloc(e->size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!data) {
            goto end;
        }
        ssize_t r = pread(spool_fd, data, e->size, (off_t) e->offset);
        if (r != (ssize_t) e->size) {
            av_free(data);
            goto end;
        }
        memset(data + e->size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
        if (av_packet_from_data(packet, data, e->size) < 0) {
            av_free(data);
            goto end;
        }
        packet->pts = e->pts - base;
        packet->dts = packet->pts;
        int64_t duration = packet_duration_us(entries, count, i, end_us);
        if (duration <= 0) {
            av_packet_unref(packet);
            goto end;
        }
        // Keep every source PTS intact. On a static screen there may be no
        // later encoded packet, so the final sample itself must carry the
        // elapsed time through the requested clip boundary.
        packet->duration = duration;
        packet->flags = e->key ? AV_PKT_FLAG_KEY : 0;
        packet->stream_index = 0;
        av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, stream->time_base);
        int w = av_write_frame(ctx, packet);
        av_packet_unref(packet);
        if (w < 0) {
            goto end;
        }
        if (avio_tell(ctx->pb) > (int64_t) SC_DAEMON_MAX_BINARY_PAYLOAD) {
            ret = SC_CLIP_ETOOLARGE;
            goto end;
        }
    }

    if (av_write_trailer(ctx) < 0) {
        goto end;
    }
    header_written = false;
    if (avio_tell(ctx->pb) > (int64_t) SC_DAEMON_MAX_BINARY_PAYLOAD) {
        ret = SC_CLIP_ETOOLARGE;
        goto end;
    }

    int size = avio_close_dyn_buf(ctx->pb, out);
    ctx->pb = NULL;
    if (size < 0) {
        goto end;
    }
    *out_size = (size_t) size;
    ret = 0;

end:
    av_packet_free(&packet);
    if (ctx) {
        if (header_written && ret != SC_CLIP_ETOOLARGE) {
            av_write_trailer(ctx);
        }
        if (ctx->pb) {
            uint8_t *junk;
            avio_close_dyn_buf(ctx->pb, &junk);
            av_free(junk);
            ctx->pb = NULL;
        }
        avformat_free_context(ctx);
    }
    return ret;
}

int
sc_clip_buffer_extract(struct sc_clip_buffer *cb, int64_t start_ms,
                       int64_t end_ms, int64_t available_end_ms,
                       uint8_t **out, size_t *out_size,
                       const struct sc_clip_format **out_format,
                       int64_t *actual_start_ms, int64_t *actual_end_ms,
                       int64_t *source_end_ms, int64_t *held_tail_ms,
                       char *errbuf, size_t errbuf_size) {
    assert(start_ms >= 0 && end_ms > start_ms);

    int64_t first;
    if (!sc_frame_keeper_get_timeline_anchor(cb->timeline, NULL, &first)) {
        snprintf(errbuf, errbuf_size,
                 "no decoded video frame has been recorded yet");
        return SC_CLIP_ERANGE;
    }

    sc_mutex_lock(&cb->mutex);

    if (cb->failed) {
        snprintf(errbuf, errbuf_size,
                 "clip buffer failed during recording; extraction is "
                 "unavailable");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_EINTERNAL;
    }
    if (cb->fd == -1 || !cb->count || !cb->epoch_count) {
        snprintf(errbuf, errbuf_size, "no video has been recorded yet");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ERANGE;
    }

    if (end_ms > available_end_ms) {
        snprintf(errbuf, errbuf_size,
                 "clip end %" PRId64 ".%03" PRId64 "s is beyond the recorded "
                 "%" PRId64 ".%03" PRId64 "s timeline",
                 end_ms / 1000, end_ms % 1000,
                 available_end_ms / 1000, available_end_ms % 1000);
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ERANGE;
    }

    int64_t start_us = first + start_ms * 1000;
    int64_t end_us = first + end_ms * 1000;

    // Packets preceding the first decoded frame are decoder preroll, not
    // visible negative report time.
    size_t timeline_begin;
    int origin_ret =
        find_timeline_origin(cb->entries, cb->count, first, &timeline_begin);
    if (origin_ret == SC_CLIP_ERANGE) {
        snprintf(errbuf, errbuf_size,
                 "no video packets at or after the first decoded frame");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ERANGE;
    }
    if (origin_ret == SC_CLIP_EINTERNAL) {
        // Starting before the decoded-frame origin would invent negative
        // report time; starting after it would omit visible content. Refuse
        // the clip instead of silently compromising either invariant.
        snprintf(errbuf, errbuf_size,
                 "first decoded frame has no matching encoded keyframe");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_EINTERNAL;
    }

    const struct sc_clip_entry *timeline_entries =
        &cb->entries[timeline_begin];
    size_t timeline_count = cb->count - timeline_begin;
    size_t begin_rel;
    size_t stop_rel;
    int64_t boundary_us = 0;
    int select_ret =
        sc_clip_select_epoch(timeline_entries, timeline_count, start_us,
                             end_us, &begin_rel, &stop_rel, &boundary_us);
    size_t begin = 0;
    size_t stop = 0;
    if (select_ret == SC_CLIP_ESESSION) {
        // A keyframe-only encoder refresh (for example an explicit freshness
        // reset) is transparent when codec parameters and config are exactly
        // unchanged. Geometry/config changes remain hard boundaries.
        if (!sc_clip_select(timeline_entries, timeline_count, start_us, end_us,
                            &begin_rel, &stop_rel)) {
            select_ret = SC_CLIP_ERANGE;
        } else {
            select_ret = 0;
            begin = timeline_begin + begin_rel;
            stop = timeline_begin + stop_rel;
            uint32_t previous_index = cb->entries[begin].epoch;
            if (previous_index >= cb->epoch_count) {
                select_ret = SC_CLIP_EINTERNAL;
            }
            for (size_t i = begin + 1; !select_ret && i <= stop; ++i) {
                uint32_t current_index = cb->entries[i].epoch;
                if (current_index == previous_index) {
                    continue;
                }
                if (current_index >= cb->epoch_count
                        || !epochs_compatible(&cb->epochs[previous_index],
                                              &cb->epochs[current_index])) {
                    boundary_us = cb->entries[i].pts;
                    select_ret = SC_CLIP_ESESSION;
                    break;
                }
                previous_index = current_index;
            }
        }
    } else if (!select_ret) {
        begin = timeline_begin + begin_rel;
        stop = timeline_begin + stop_rel;
    }
    if (select_ret == SC_CLIP_ESESSION) {
        int64_t boundary_ms = (boundary_us - first) / 1000;
        snprintf(errbuf, errbuf_size,
                 "clip crosses an incompatible codec/geometry boundary at "
                 "%" PRId64 ".%03" PRId64
                 "s; split the request at that boundary",
                 boundary_ms / 1000, boundary_ms % 1000);
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ESESSION;
    }
    if (select_ret == SC_CLIP_EINTERNAL) {
        snprintf(errbuf, errbuf_size, "invalid video session index");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_EINTERNAL;
    }
    if (select_ret) {
        snprintf(errbuf, errbuf_size, "no video packets in the requested "
                                      "range");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ERANGE;
    }

    uint32_t epoch_index = cb->entries[begin].epoch;
    if (epoch_index >= cb->epoch_count) {
        snprintf(errbuf, errbuf_size, "invalid video session index");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_EINTERNAL;
    }
    const struct sc_clip_epoch *epoch = &cb->epochs[epoch_index];
    const struct sc_clip_format *format =
        sc_clip_format_for_codec(epoch->par->codec_id);
    if (!format) {
        snprintf(errbuf, errbuf_size, "unsupported video codec id %d",
                 epoch->par->codec_id);
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_EINTERNAL;
    }

    // Reject only when the raw encoded payload itself already exceeds the
    // protocol bound. Container overhead is format-dependent, so the exact
    // limit is enforced below while muxing instead of pessimistically
    // rejecting a clip whose actual output would fit.
    size_t raw_size = epoch->config_size;
    if (raw_size > SC_DAEMON_MAX_BINARY_PAYLOAD) {
        snprintf(errbuf, errbuf_size,
                 "clip exceeds the 1 GiB daemon payload limit; split the "
                 "requested range");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ETOOLARGE;
    }
    for (size_t i = begin; i <= stop; ++i) {
        size_t packet_size = cb->entries[i].size;
        if (packet_size > SC_DAEMON_MAX_BINARY_PAYLOAD - raw_size) {
            snprintf(errbuf, errbuf_size,
                     "clip exceeds the 1 GiB daemon payload limit; split the "
                     "requested range");
            sc_mutex_unlock(&cb->mutex);
            return SC_CLIP_ETOOLARGE;
        }
        raw_size += packet_size;
    }

    // Snapshot the selected slice, epoch parameters/config and the spool fd,
    // then release the lock. dup() keeps this exact unlinked spool alive even
    // if a reconnect resets the buffer while muxing.
    size_t n = stop - begin + 1;
    struct sc_clip_entry *slice = malloc(n * sizeof(*slice));
    uint8_t *config = NULL;
    size_t config_size = epoch->config_size;
    if (config_size) {
        config = malloc(config_size);
    }
    AVCodecParameters *par = avcodec_parameters_alloc();
    bool par_ok = par && avcodec_parameters_copy(par, epoch->par) >= 0;
    int spool_fd = dup(cb->fd);
    if (!slice || (config_size && !config) || !par_ok || spool_fd == -1) {
        free(slice);
        free(config);
        avcodec_parameters_free(&par);
        if (spool_fd != -1) {
            close(spool_fd);
        }
        snprintf(errbuf, errbuf_size, "could not snapshot the video buffer");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_EINTERNAL;
    }
    memcpy(slice, &cb->entries[begin], n * sizeof(*slice));
    if (config_size) {
        memcpy(config, epoch->config, config_size);
    }
    sc_mutex_unlock(&cb->mutex);

    int ret = mux_entries(spool_fd, par, slice, n, config, config_size, end_us,
                          format, out, out_size);
    close(spool_fd);
    avcodec_parameters_free(&par);
    if (ret) {
        if (ret == SC_CLIP_ETOOLARGE) {
            snprintf(errbuf, errbuf_size,
                     "clip exceeds the 1 GiB daemon payload limit; split the "
                     "requested range");
        } else {
            snprintf(errbuf, errbuf_size, "could not mux the clip");
        }
    } else {
        *out_format = format;
        *actual_start_ms = (slice[0].pts - first) / 1000;
        *actual_end_ms = end_ms;
        *source_end_ms = (slice[n - 1].pts - first) / 1000;
        *held_tail_ms = end_ms - *source_end_ms;
    }
    free(slice);
    free(config);
    return ret;
}
