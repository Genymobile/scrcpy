#ifndef SC_DAEMON_TOUCH_REPORT_H
#define SC_DAEMON_TOUCH_REPORT_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#include "android/input.h"
#include "util/tick.h"

// Android accepts at most 10 simultaneous pointers (PointersState.MAX_POINTERS).
#define SC_TOUCH_REPORT_MAX_POINTERS 10

struct sc_touch_report_slot {
    bool active;
    uint64_t pointer_id;
    int32_t start_x;
    int32_t start_y;
    sc_tick start_tick;
    uint32_t sample_count;
};

struct sc_touch_report {
    struct sc_touch_report_slot slots[SC_TOUCH_REPORT_MAX_POINTERS];
};

struct sc_touch_report_summary {
    uint64_t pointer_id;
    int32_t start_x;
    int32_t start_y;
    int32_t x;
    int32_t y;
    int64_t duration_ms;
    uint32_t sample_count;
};

void
sc_touch_report_init(struct sc_touch_report *report);

/**
 * Feed one accepted realtime touch sample.
 *
 * Samples are grouped by pointer id. Down starts (or restarts) a gesture,
 * move extends it, and up emits exactly one summary and clears the slot.
 * A move/up without a preceding down starts at that sample, matching the
 * daemon's tolerant realtime-input behavior. Hover events are ignored.
 *
 * Returns true and populates `out` only for a terminating up sample.
 */
bool
sc_touch_report_feed(struct sc_touch_report *report,
                     enum android_motionevent_action action,
                     uint64_t pointer_id, int32_t x, int32_t y, sc_tick tick,
                     struct sc_touch_report_summary *out);

#endif
