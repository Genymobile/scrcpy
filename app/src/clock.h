#ifndef SC_CLOCK_H
#define SC_CLOCK_H

#include "common.h"

#include "util/tick.h"

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
 * Theoretically, the slope encodes the drift between the device clock and the
 * computer clock. It is expected to be very close to 1.
 *
 * Since the clock is used to estimate very close points in the future (which
 * are reestimated on every clock update, see delay_buffer), the error caused
 * by clock drift is totally negligible, so it is better to assume that the
 * slope is 1 than to estimate it (the estimation error would be larger).
 *
 * Therefore, only the offset is estimated.
 */
struct sc_clock {
    unsigned range;
    sc_tick offset;
};

void
sc_clock_init(struct sc_clock *clock);

void
sc_clock_update(struct sc_clock *clock, sc_tick system, sc_tick stream);

sc_tick
sc_clock_to_system_time(struct sc_clock *clock, sc_tick stream);

#endif
