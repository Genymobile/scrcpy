#include "touch_report.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static struct sc_touch_report_slot *
find_slot(struct sc_touch_report *report, uint64_t pointer_id) {
    for (unsigned i = 0; i < SC_TOUCH_REPORT_MAX_POINTERS; ++i) {
        struct sc_touch_report_slot *slot = &report->slots[i];
        if (slot->active && slot->pointer_id == pointer_id) {
            return slot;
        }
    }
    return NULL;
}

static struct sc_touch_report_slot *
find_free_slot(struct sc_touch_report *report) {
    for (unsigned i = 0; i < SC_TOUCH_REPORT_MAX_POINTERS; ++i) {
        struct sc_touch_report_slot *slot = &report->slots[i];
        if (!slot->active) {
            return slot;
        }
    }
    return NULL;
}

static void
start_slot(struct sc_touch_report_slot *slot, uint64_t pointer_id, int32_t x,
           int32_t y, sc_tick tick) {
    slot->active = true;
    slot->pointer_id = pointer_id;
    slot->start_x = x;
    slot->start_y = y;
    slot->start_tick = tick;
    slot->sample_count = 0;
}

void
sc_touch_report_init(struct sc_touch_report *report) {
    memset(report, 0, sizeof(*report));
}

bool
sc_touch_report_feed(struct sc_touch_report *report,
                     enum android_motionevent_action action,
                     uint64_t pointer_id, int32_t x, int32_t y, sc_tick tick,
                     struct sc_touch_report_summary *out) {
    assert(report);
    assert(out);

    if (action != AMOTION_EVENT_ACTION_DOWN
            && action != AMOTION_EVENT_ACTION_MOVE
            && action != AMOTION_EVENT_ACTION_UP) {
        return false;
    }

    struct sc_touch_report_slot *slot = find_slot(report, pointer_id);
    if (action == AMOTION_EVENT_ACTION_DOWN || !slot) {
        if (!slot) {
            slot = find_free_slot(report);
            if (!slot) {
                return false;
            }
        }
        start_slot(slot, pointer_id, x, y, tick);
    }

    if (slot->sample_count < UINT32_MAX) {
        ++slot->sample_count;
    }

    if (action != AMOTION_EVENT_ACTION_UP) {
        return false;
    }

    sc_tick elapsed = tick >= slot->start_tick ? tick - slot->start_tick : 0;
    *out = (struct sc_touch_report_summary) {
        .pointer_id = pointer_id,
        .start_x = slot->start_x,
        .start_y = slot->start_y,
        .x = x,
        .y = y,
        .duration_ms = SC_TICK_TO_MS(elapsed),
        .sample_count = slot->sample_count,
    };
    slot->active = false;
    return true;
}
