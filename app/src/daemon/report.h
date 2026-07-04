#ifndef SC_DAEMON_REPORT_H
#define SC_DAEMON_REPORT_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "daemon/frame_keeper.h"
#include "options.h"
#include "recorder.h"
#include "util/thread.h"

/**
 * Test-report writer (DESIGN-test-report.md).
 *
 * When the daemon runs with --auto-test-report=DIR, this module:
 *  - records the device screen to DIR/recording.mp4 (via sc_recorder, a packet
 *    sink on the video demuxer),
 *  - appends every logged client operation to DIR/events.jsonl with a
 *    frame-accurate, drift-free timestamp derived from the video PTS clock,
 *  - writes DIR/manifest.json at start and finalizes it on stop.
 *
 * Lifecycle: init() once at daemon start; start_recording()/stop_recording()
 * around the device session; log_event() from the request dispatch; destroy()
 * at daemon shutdown.
 */
struct sc_report {
    char *dir;
    char *video_path; // <dir>/recording.mp4

    struct sc_frame_keeper *keeper; // not owned; source of the video-time clock

    struct sc_recorder recorder;
    bool recorder_initialized;
    bool recorder_started;
    bool recorder_failed; // set from the recorder callback

    FILE *events; // events.jsonl (append)
    sc_mutex mutex; // guards `events`, `seq`, `recorder_failed`
    uint64_t seq;

    char serial[128];
    char device_name[128];
};

/**
 * Create the report directory and open the event log. Does not start the
 * recorder (that happens per-session). Returns false on failure.
 */
bool
sc_report_init(struct sc_report *report, const char *dir,
               struct sc_frame_keeper *keeper, const char *serial,
               const char *device_name);

void
sc_report_destroy(struct sc_report *report);

/**
 * Initialize and start the screen recorder for the current session. On
 * success, the recorder's video packet sink must be added to the video
 * demuxer's packet source by the caller, before the demuxer starts.
 */
bool
sc_report_start_recording(struct sc_report *report, bool video,
                          enum sc_orientation orientation);

/** The recorder's video packet sink, valid after start_recording(). */
struct sc_packet_sink *
sc_report_video_sink(struct sc_report *report);

/**
 * Stop, join and destroy the recorder (finalizes recording.mp4), and write the
 * final manifest. Safe to call once after start_recording().
 */
void
sc_report_stop_recording(struct sc_report *report);

/**
 * Append one operation to the event log.
 *
 * `op` is the operation name ("control", "screencap", "inject_touch", ...).
 * `action` is the optional --action text (may be NULL).
 * `extra_json` is an already-formatted JSON fragment of op-specific fields
 * without surrounding braces or a leading comma (may be NULL), e.g.
 *   "\"video_size\":{\"w\":1080,\"h\":2400},\"cmds\":[\"click 1 2\"]"
 *
 * The common fields (seq, t_ms, wall, op, action) are added automatically.
 * The timestamp is the current video position (frame-accurate).
 */
void
sc_report_log_event(struct sc_report *report, const char *op,
                    const char *action, const char *extra_json);

#endif
