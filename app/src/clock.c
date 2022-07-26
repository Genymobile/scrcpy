#include "clock.h"

#include "util/log.h"

#define SC_CLOCK_NDEBUG // comment to debug

void
sc_clock_init(struct sc_clock *clock) {
    clock->count = 0;
    clock->head = 0;
    clock->left_sum.system = 0;
    clock->left_sum.stream = 0;
    clock->right_sum.system = 0;
    clock->right_sum.stream = 0;
}

// Estimate the affine function f(stream) = slope * stream + offset
static void
sc_clock_estimate(struct sc_clock *clock,
                  double *out_slope, sc_tick *out_offset) {
    assert(clock->count > 1); // two points are necessary

    struct sc_clock_point left_avg = {
        .system = clock->left_sum.system / (clock->count / 2),
        .stream = clock->left_sum.stream / (clock->count / 2),
    };
    struct sc_clock_point right_avg = {
        .system = clock->right_sum.system / ((clock->count + 1) / 2),
        .stream = clock->right_sum.stream / ((clock->count + 1) / 2),
    };

    double slope = (double) (right_avg.system - left_avg.system)
                 / (right_avg.stream - left_avg.stream);

    if (clock->count < SC_CLOCK_RANGE) {
        /* The first frames are typically received and decoded with more delay
         * than the others, causing a wrong slope estimation on start. To
         * compensate, assume an initial slope of 1, then progressively use the
         * estimated slope. */
        slope = (clock->count * slope + (SC_CLOCK_RANGE - clock->count))
              / SC_CLOCK_RANGE;
    }

    struct sc_clock_point global_avg = {
        .system = (clock->left_sum.system + clock->right_sum.system)
                 / clock->count,
        .stream = (clock->left_sum.stream + clock->right_sum.stream)
                 / clock->count,
    };

    sc_tick offset = global_avg.system - (sc_tick) (global_avg.stream * slope);

    *out_slope = slope;
    *out_offset = offset;
}

void
sc_clock_update(struct sc_clock *clock, sc_tick system, sc_tick stream) {
    struct sc_clock_point *point = &clock->points[clock->head];

    if (clock->count == SC_CLOCK_RANGE || clock->count & 1) {
        // One point passes from the right sum to the left sum

        unsigned mid;
        if (clock->count == SC_CLOCK_RANGE) {
            mid = (clock->head + SC_CLOCK_RANGE / 2) % SC_CLOCK_RANGE;
        } else {
            // Only for the first frames
            mid = clock->count / 2;
        }

        struct sc_clock_point *mid_point = &clock->points[mid];
        clock->left_sum.system += mid_point->system;
        clock->left_sum.stream += mid_point->stream;
        clock->right_sum.system -= mid_point->system;
        clock->right_sum.stream -= mid_point->stream;
    }

    if (clock->count == SC_CLOCK_RANGE) {
        // The current point overwrites the previous value in the circular
        // array, update the left sum accordingly
        clock->left_sum.system -= point->system;
        clock->left_sum.stream -= point->stream;
    } else {
        ++clock->count;
    }

    point->system = system;
    point->stream = stream;

    clock->right_sum.system += system;
    clock->right_sum.stream += stream;

    clock->head = (clock->head + 1) % SC_CLOCK_RANGE;

    if (clock->count > 1) {
        // Update estimation
        sc_clock_estimate(clock, &clock->slope, &clock->offset);

#ifndef SC_CLOCK_NDEBUG
        LOGD("Clock estimation: %f * pts + %" PRItick,
             clock->slope, clock->offset);
#endif
    }
}

sc_tick
sc_clock_to_system_time(struct sc_clock *clock, sc_tick stream) {
    assert(clock->count > 1); // sc_clock_update() must have been called
    return (sc_tick) (stream * clock->slope) + clock->offset;
}
