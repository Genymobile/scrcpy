#include "clip_buffer.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libavformat/avformat.h>

#include "util/log.h"

/** Downcast packet_sink to sc_clip_buffer */
#define DOWNCAST(SINK) container_of(SINK, struct sc_clip_buffer, packet_sink)

static const AVRational SCRCPY_TIME_BASE = {1, 1000000}; // timestamps in us

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
    free(cb->config);
    cb->config = NULL;
    cb->config_size = 0;
    free(cb->entries);
    cb->entries = NULL;
    cb->count = 0;
    cb->cap = 0;
    if (cb->par) {
        avcodec_parameters_free(&cb->par);
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

bool
sc_clip_buffer_init(struct sc_clip_buffer *cb) {
    memset(cb, 0, sizeof(*cb));
    cb->fd = -1;

    static const struct sc_packet_sink_ops ops = {
        .open = sc_clip_buffer_packet_sink_open,
        .close = sc_clip_buffer_packet_sink_close,
        .push = sc_clip_buffer_packet_sink_push,
    };
    cb->packet_sink.ops = &ops;

    return sc_mutex_init(&cb->mutex);
}

void
sc_clip_buffer_destroy(struct sc_clip_buffer *cb) {
    reset_locked(cb);
    sc_mutex_destroy(&cb->mutex);
}

// ---- packet sink trait ------------------------------------------------------

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
    cb->par = avcodec_parameters_alloc();
    if (cb->par && avcodec_parameters_from_context(cb->par, ctx) < 0) {
        avcodec_parameters_free(&cb->par);
    }
    cb->fd = create_spool();
    cb->opened = cb->par && cb->fd != -1;
    if (!cb->opened) {
        LOGW("Clip buffer: unavailable for this session");
        reset_locked(cb);
    }
    sc_mutex_unlock(&cb->mutex);
    // Never fail the demuxer pipeline: without a spool, clip requests will
    // simply return an error
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
        uint8_t *config = malloc(packet->size);
        if (config) {
            memcpy(config, packet->data, packet->size);
            free(cb->config);
            cb->config = config;
            cb->config_size = packet->size;
        }
        goto out;
    }

    if (cb->count == cb->cap) {
        size_t cap = cb->cap ? cb->cap * 2 : 1024;
        struct sc_clip_entry *entries =
            realloc(cb->entries, cap * sizeof(*entries));
        if (!entries) {
            goto out;
        }
        cb->entries = entries;
        cb->cap = cap;
    }

    ssize_t w = pwrite(cb->fd, packet->data, packet->size,
                       (off_t) cb->spool_size);
    if (w != packet->size) {
        // Disk full or I/O error: disable the buffer, keep the session alive
        LOGW("Clip buffer: spool write failed, clips disabled");
        close(cb->fd);
        cb->fd = -1;
        goto out;
    }

    struct sc_clip_entry *e = &cb->entries[cb->count++];
    e->pts = packet->pts;
    e->offset = cb->spool_size;
    e->size = packet->size;
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

bool
sc_clip_select(const struct sc_clip_entry *entries, size_t count,
               int64_t start_us, int64_t end_us, size_t *begin, size_t *stop) {
    if (!count || start_us > end_us) {
        return false;
    }

    ssize_t last = last_at_or_before(entries, count, end_us);
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

// ---- extraction -------------------------------------------------------------

static int
mux_entries(struct sc_clip_buffer *cb, const struct sc_clip_entry *entries,
            size_t count, const uint8_t *config, size_t config_size,
            uint8_t **out, size_t *out_size) {
    int ret = SC_CLIP_EINTERNAL;
    AVFormatContext *ctx = NULL;
    AVPacket *packet = NULL;
    bool header_written = false;

    if (avformat_alloc_output_context2(&ctx, NULL, "mp4", NULL) < 0) {
        return SC_CLIP_EINTERNAL;
    }

    AVStream *stream = avformat_new_stream(ctx, NULL);
    if (!stream || avcodec_parameters_copy(stream->codecpar, cb->par) < 0) {
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
        ssize_t r = pread(cb->fd, data, e->size, (off_t) e->offset);
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
        packet->flags = e->key ? AV_PKT_FLAG_KEY : 0;
        packet->stream_index = 0;
        av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, stream->time_base);
        int w = av_write_frame(ctx, packet);
        av_packet_unref(packet);
        if (w < 0) {
            goto end;
        }
    }

    if (av_write_trailer(ctx) < 0) {
        goto end;
    }
    header_written = false;

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
        if (header_written) {
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
                       int64_t end_ms, uint8_t **out, size_t *out_size,
                       int64_t *actual_start_ms, int64_t *actual_end_ms,
                       char *errbuf, size_t errbuf_size) {
    assert(start_ms >= 0 && end_ms > start_ms);

    sc_mutex_lock(&cb->mutex);

    if (cb->fd == -1 || !cb->count || !cb->par) {
        snprintf(errbuf, errbuf_size, "no video has been recorded yet");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ERANGE;
    }

    int64_t first = cb->entries[0].pts;
    int64_t last = cb->entries[cb->count - 1].pts;
    int64_t start_us = first + start_ms * 1000;
    int64_t end_us = first + end_ms * 1000;

    if (end_us > last) {
        snprintf(errbuf, errbuf_size,
                 "clip end %" PRId64 ".%03" PRId64 "s is beyond the recorded "
                 "%" PRId64 ".%03" PRId64 "s",
                 end_ms / 1000, end_ms % 1000,
                 (last - first) / 1000000, ((last - first) / 1000) % 1000);
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ERANGE;
    }

    size_t begin, stop;
    if (!sc_clip_select(cb->entries, cb->count, start_us, end_us,
                        &begin, &stop)) {
        snprintf(errbuf, errbuf_size, "no video packets in the requested "
                                      "range");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_ERANGE;
    }

    // Snapshot the selected index slice and the config packet, then release
    // the lock: spool data is append-only, so the slice is immutable and the
    // (slow) muxing must not stall the demuxer
    size_t n = stop - begin + 1;
    struct sc_clip_entry *slice = malloc(n * sizeof(*slice));
    uint8_t *config = NULL;
    size_t config_size = cb->config_size;
    if (config_size) {
        config = malloc(config_size);
    }
    if (!slice || (config_size && !config)) {
        free(slice);
        free(config);
        snprintf(errbuf, errbuf_size, "out of memory");
        sc_mutex_unlock(&cb->mutex);
        return SC_CLIP_EINTERNAL;
    }
    memcpy(slice, &cb->entries[begin], n * sizeof(*slice));
    if (config_size) {
        memcpy(config, cb->config, config_size);
    }
    sc_mutex_unlock(&cb->mutex);

    int ret = mux_entries(cb, slice, n, config, config_size, out, out_size);
    if (ret) {
        snprintf(errbuf, errbuf_size, "could not mux the clip");
    } else {
        *actual_start_ms = (slice[0].pts - first) / 1000;
        *actual_end_ms = (slice[n - 1].pts - first) / 1000;
    }
    free(slice);
    free(config);
    return ret;
}

