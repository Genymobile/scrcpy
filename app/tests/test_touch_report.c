#include "common.h"

#include <assert.h>
#include <stdint.h>

#include "control_msg.h"
#include "daemon/touch_report.h"

static void
test_gesture_summary(void) {
    struct sc_touch_report report;
    sc_touch_report_init(&report);
    struct sc_touch_report_summary summary;

    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_DOWN, 7,
                                 100, 200, SC_TICK_FROM_MS(10), &summary));
    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_MOVE, 7,
                                 150, 250, SC_TICK_FROM_MS(110), &summary));
    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_MOVE, 7,
                                 220, 320, SC_TICK_FROM_MS(260), &summary));
    assert(sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_UP, 7,
                                300, 400, SC_TICK_FROM_MS(510), &summary));

    assert(summary.pointer_id == 7);
    assert(summary.start_x == 100);
    assert(summary.start_y == 200);
    assert(summary.x == 300);
    assert(summary.y == 400);
    assert(summary.duration_ms == 500);
    assert(summary.sample_count == 4);
}

static void
test_interleaved_pointers(void) {
    struct sc_touch_report report;
    sc_touch_report_init(&report);
    struct sc_touch_report_summary summary;

    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_DOWN, 1,
                                 10, 20, SC_TICK_FROM_MS(0), &summary));
    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_DOWN, 2,
                                 30, 40, SC_TICK_FROM_MS(5), &summary));
    assert(sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_UP, 1,
                                50, 60, SC_TICK_FROM_MS(10), &summary));
    assert(summary.pointer_id == 1);
    assert(summary.start_x == 10);
    assert(summary.sample_count == 2);

    assert(sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_UP, 2,
                                70, 80, SC_TICK_FROM_MS(25), &summary));
    assert(summary.pointer_id == 2);
    assert(summary.start_x == 30);
    assert(summary.duration_ms == 20);
    assert(summary.sample_count == 2);
}

static void
test_tolerates_missing_down_and_ignores_hover(void) {
    struct sc_touch_report report;
    sc_touch_report_init(&report);
    struct sc_touch_report_summary summary;

    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_HOVER_MOVE,
                                 SC_POINTER_ID_MOUSE, 1, 2,
                                 SC_TICK_FROM_MS(1), &summary));
    assert(sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_UP,
                                SC_POINTER_ID_MOUSE, 55, 66,
                                SC_TICK_FROM_MS(2), &summary));
    assert(summary.pointer_id == SC_POINTER_ID_MOUSE);
    assert(summary.start_x == 55);
    assert(summary.start_y == 66);
    assert(summary.x == 55);
    assert(summary.y == 66);
    assert(summary.duration_ms == 0);
    assert(summary.sample_count == 1);
}

static void
test_down_restarts_pointer(void) {
    struct sc_touch_report report;
    sc_touch_report_init(&report);
    struct sc_touch_report_summary summary;

    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_DOWN, 3,
                                 1, 2, SC_TICK_FROM_MS(1), &summary));
    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_MOVE, 3,
                                 3, 4, SC_TICK_FROM_MS(2), &summary));
    assert(!sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_DOWN, 3,
                                 10, 20, SC_TICK_FROM_MS(10), &summary));
    assert(sc_touch_report_feed(&report, AMOTION_EVENT_ACTION_UP, 3,
                                30, 40, SC_TICK_FROM_MS(30), &summary));
    assert(summary.start_x == 10);
    assert(summary.start_y == 20);
    assert(summary.duration_ms == 20);
    assert(summary.sample_count == 2);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_gesture_summary();
    test_interleaved_pointers();
    test_tolerates_missing_down_and_ignores_hover();
    test_down_restarts_pointer();
    return 0;
}
