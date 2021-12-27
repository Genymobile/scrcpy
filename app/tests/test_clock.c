#include "common.h"

#include <assert.h>

#include "clock.h"

void test_small_rolling_sum(void) {
    struct sc_clock clock;
    sc_clock_init(&clock);

    assert(clock.count == 0);
    assert(clock.left_sum.system == 0);
    assert(clock.left_sum.stream == 0);
    assert(clock.right_sum.system == 0);
    assert(clock.right_sum.stream == 0);

    sc_clock_update(&clock, 2, 3);
    assert(clock.count == 1);
    assert(clock.left_sum.system == 0);
    assert(clock.left_sum.stream == 0);
    assert(clock.right_sum.system == 2);
    assert(clock.right_sum.stream == 3);

    sc_clock_update(&clock, 10, 20);
    assert(clock.count == 2);
    assert(clock.left_sum.system == 2);
    assert(clock.left_sum.stream == 3);
    assert(clock.right_sum.system == 10);
    assert(clock.right_sum.stream == 20);

    sc_clock_update(&clock, 40, 80);
    assert(clock.count == 3);
    assert(clock.left_sum.system == 2);
    assert(clock.left_sum.stream == 3);
    assert(clock.right_sum.system == 50);
    assert(clock.right_sum.stream == 100);

    sc_clock_update(&clock, 400, 800);
    assert(clock.count == 4);
    assert(clock.left_sum.system == 12);
    assert(clock.left_sum.stream == 23);
    assert(clock.right_sum.system == 440);
    assert(clock.right_sum.stream == 880);
}

void test_large_rolling_sum(void) {
    const unsigned half_range = SC_CLOCK_RANGE / 2;

    struct sc_clock clock1;
    sc_clock_init(&clock1);
    for (unsigned i = 0; i < 5 * half_range; ++i) {
        sc_clock_update(&clock1, i, 2 * i + 1);
    }

    struct sc_clock clock2;
    sc_clock_init(&clock2);
    for (unsigned i = 3 * half_range; i < 5 * half_range; ++i) {
        sc_clock_update(&clock2, i, 2 * i + 1);
    }

    assert(clock1.count == SC_CLOCK_RANGE);
    assert(clock2.count == SC_CLOCK_RANGE);

    // The values before the last SC_CLOCK_RANGE points in clock1 should have
    // no impact
    assert(clock1.left_sum.system == clock2.left_sum.system);
    assert(clock1.left_sum.stream == clock2.left_sum.stream);
    assert(clock1.right_sum.system == clock2.right_sum.system);
    assert(clock1.right_sum.stream == clock2.right_sum.stream);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_small_rolling_sum();
    test_large_rolling_sum();
    return 0;
};
