#ifndef SC_DAEMON_CLIP_BUFFER_H
#define SC_DAEMON_CLIP_BUFFER_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>

#include "trait/packet_sink.h"
#include "util/thread.h"

struct sc_frame_keeper;

/**
 * Clip buffer (doc/daemon.md §9.5): retain the ENCODED video stream so a
 * client can extract an arbitrary [start, end] segment of the session while
 * recording continues.
 *
 * A packet sink attached to the video demuxer, fully independent of the
 * decoder/broadcaster/recorder sinks: each encoded packet is appended to an
 * unlinked spool file and indexed in memory {pts, offset, size, keyframe}.
 * On a "clip" request the daemon selects [last keyframe <= start, last
 * packet < end], muxes those packets into an in-memory codec-compatible
 * container with timestamps rebased to 0, and extends the final sample
 * duration to exactly `end`.
 * Existing packet timestamps are never moved. This represents static-screen
 * time without inventing or re-encoding frames. The live session and the
 * report recording are never touched.
 *
 * Clip times are relative to the first successfully retained decoded frame
 * (t = 0), the same origin as the report recording and event timeline.
 */

struct sc_clip_entry {
    int64_t pts;     // µs (SCRCPY_TIME_BASE)
    uint64_t offset; // byte offset in the spool file
    uint32_t size;   // packet size in bytes
    uint32_t epoch;  // stream-session epoch (resolution/config generation)
    bool key;        // keyframe
};

struct sc_clip_epoch {
    AVCodecParameters *par;
    uint8_t *config;
    size_t config_size;
    size_t first_entry;
};

struct sc_clip_buffer {
    struct sc_packet_sink packet_sink; // packet sink trait

    sc_mutex mutex; // guards everything below (demuxer append vs extract)

    int fd;              // unlinked spool file, -1 when unavailable
    uint64_t spool_size; // bytes written so far

    // One codec/config snapshot per dynamic stream session. Keeping all
    // epochs prevents a later encoder reset from corrupting older clips with
    // newer SPS/geometry.
    struct sc_clip_epoch *epochs;
    size_t epoch_count;
    size_t epoch_cap;
    uint32_t current_epoch;

    struct sc_clip_entry *entries; // sorted by pts (monotonic stream)
    size_t count;
    size_t cap;

    // Not owned. Supplies the immutable (host tick, media PTS) anchor of the
    // first successfully retained decoded frame.
    struct sc_frame_keeper *timeline;

    bool opened;
    bool failed; // permanent for this device session; extract is untrusted
};

struct sc_clip_format {
    const char *muxer_name; // FFmpeg muxer name
    const char *container;  // protocol value: "mp4" or "webm"
    const char *extension;  // recommended file extension, including '.'
    const char *mime_type;
};

/**
 * Select a lossless/remux-only output container for an encoded codec.
 *
 * Upstream scrcpy 4.1 explicitly rejects VP8 in MP4, so VP8 uses WebM.
 * The other supported codecs, including VP9, continue to use MP4.
 * Returns NULL for an unsupported codec.
 */
const struct sc_clip_format *
sc_clip_format_for_codec(enum AVCodecID codec_id);

bool
sc_clip_buffer_init(struct sc_clip_buffer *cb,
                    struct sc_frame_keeper *timeline);

void
sc_clip_buffer_destroy(struct sc_clip_buffer *cb);

/**
 * Last encoded packet timestamp in milliseconds relative to the first decoded
 * frame.
 * Unlike the session/report clock, this value may remain unchanged while the
 * screen is static. Returns false if no video packet has been received yet.
 */
bool
sc_clip_buffer_source_time_ms(struct sc_clip_buffer *cb, int64_t *out_ms);

/**
 * Current real-time recording position, in milliseconds, anchored at the
 * first successfully retained decoded frame. It continues while the screen is
 * static and freezes when the frame sink closes.
 */
bool
sc_clip_buffer_timeline_time_ms(struct sc_clip_buffer *cb, int64_t *out_ms);

/**
 * Pure index selection for the half-open window [start_us, end_us) (µs, same
 * scale as the entries): *begin = the keyframe at or before start_us (walking
 * forward to the first keyframe if the stream starts later), *stop = the last
 * entry with pts < end_us. Returns false when the window selects nothing.
 * Exposed for unit tests.
 */
bool
sc_clip_select(const struct sc_clip_entry *entries, size_t count,
               int64_t start_us, int64_t end_us, size_t *begin, size_t *stop);

#define SC_CLIP_ERANGE (-1)    // range not (yet) recorded
#define SC_CLIP_EINTERNAL (-2) // I/O or muxing failure
#define SC_CLIP_ESESSION (-3)  // range crosses a stream-session boundary
#define SC_CLIP_ETOOLARGE (-4) // muxed payload exceeds protocol bound

/**
 * Select a clip without crossing a stream-session epoch.
 *
 * Returns 0 on success, SC_CLIP_ERANGE when the range contains no selectable
 * packets, or SC_CLIP_ESESSION when it spans two codec/geometry epochs. On
 * SC_CLIP_ESESSION, *boundary_us is the first packet timestamp of the epoch
 * selected by the range end.
 */
int
sc_clip_select_epoch(const struct sc_clip_entry *entries, size_t count,
                     int64_t start_us, int64_t end_us, size_t *begin,
                     size_t *stop, int64_t *boundary_us);

#ifdef SC_TEST
// Expose the mux duration rule for focused unit tests.
int64_t
sc_clip_packet_duration_us(const struct sc_clip_entry *entries, size_t count,
                           size_t index, int64_t end_us);

// Locate the exact encoded keyframe corresponding to the decoded-frame
// timeline origin. Decoder preroll before it is permitted; a missing/non-key
// origin is an invariant failure.
int
sc_clip_find_timeline_origin(const struct sc_clip_entry *entries, size_t count,
                             int64_t origin, size_t *out_index);

// Exact compatibility rule used to decide whether adjacent encoder epochs may
// remain in one lossless-remuxed clip.
bool
sc_clip_epochs_compatible(const struct sc_clip_epoch *a,
                          const struct sc_clip_epoch *b);
#endif

/**
 * Extract [start_ms, end_ms) into a standalone codec-compatible container.
 * `available_end_ms` is the wall-elapsed session position from
 * sc_clip_buffer_timeline_time_ms(); it is the sole authority for whether
 * `end_ms` has been recorded. Encoded packet PTS may stop during a static
 * screen and must not shorten the timeline.
 *
 * On success returns 0 and sets *out (caller frees with av_free()), *out_size,
 * the selected output format, the actual bounds (start snaps back to a
 * keyframe; end remains the requested end), the last source packet position,
 * and the duration for which that last sample is held. On failure returns
 * SC_CLIP_ERANGE, SC_CLIP_ESESSION or SC_CLIP_EINTERNAL and writes a
 * human-readable reason to errbuf.
 */
int
sc_clip_buffer_extract(struct sc_clip_buffer *cb, int64_t start_ms,
                       int64_t end_ms, int64_t available_end_ms,
                       uint8_t **out, size_t *out_size,
                       const struct sc_clip_format **out_format,
                       int64_t *actual_start_ms, int64_t *actual_end_ms,
                       int64_t *source_end_ms, int64_t *held_tail_ms,
                       char *errbuf, size_t errbuf_size);

#endif
