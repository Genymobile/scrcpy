#ifndef SC_CLOCK_H
#define SC_CLOCK_H

#include "common.h"

#include <assert.h>

#include "util/tick.h"

#define SC_CLOCK_RANGE 32
static_assert(!(SC_CLOCK_RANGE & 1), "SC_CLOCK_RANGE must be even");

struct sc_clock_point {
    sc_tick system;
    sc_tick stream;
};

/**
 * The clock aims to estimate the affine relation between the stream (device)
 * time and the system time:
 *
 *     f(stream) = slope * stream + offset
 *
 * To that end, it stores the SC_CLOCK_RANGE last clock points (the timestamps
 * of a frame expressed both in stream time and system time) in a circular
 * array.
 *
 * To estimate the slope, it splits the last SC_CLOCK_RANGE points into two
 * sets of SC_CLOCK_RANGE/2 points, and computes their centroid ("average
 * point"). The slope of the estimated affine function is that of the line
 * passing through these two points.
 *
 * To estimate the offset, it computes the centroid of all the SC_CLOCK_RANGE
 * points. The resulting affine function passes by this centroid.
 *
 * With a circular array, the rolling sums (and average) are quick to compute.
 * In practice, the estimation is stable and the evolution is smooth.
 */
struct sc_clock {
    // Circular array
    struct sc_clock_point points[SC_CLOCK_RANGE];

    // Number of points in the array (count <= SC_CLOCK_RANGE)
    unsigned count;

    // Index of the next point to write
    unsigned head;

    // Sum of the first count/2 points
    struct sc_clock_point left_sum;

    // Sum of the last (count+1)/2 points
    struct sc_clock_point right_sum;

    // Estimated slope and offset
    // (computed on sc_clock_update(), used by sc_clock_to_system_time())
    double slope;
    sc_tick offset;
};

void
sc_clock_init(struct sc_clock *clock);

void
sc_clock_update(struct sc_clock *clock, sc_tick system, sc_tick stream);

sc_tick
sc_clock_to_system_time(struct sc_clock *clock, sc_tick stream);

#endif
