#include "clock.h"

#include <assert.h>

#include "util/log.h"

//#define SC_CLOCK_DEBUG // uncomment to debug

#define SC_CLOCK_RANGE 32

void
sc_clock_init(struct sc_clock *clock) {
    clock->range = 0;
    clock->offset = 0;
}

void
sc_clock_update(struct sc_clock *clock, sc_tick system, sc_tick stream) {
    if (clock->range < SC_CLOCK_RANGE) {
        ++clock->range;
    }

    sc_tick offset = system - stream;
    unsigned clock_weight = clock->range - 1;
    unsigned value_weight = SC_CLOCK_RANGE - clock->range + 1;
    clock->offset = (clock->offset * clock_weight + offset * value_weight)
                  / SC_CLOCK_RANGE;

#ifdef SC_CLOCK_DEBUG
    LOGD("Clock estimation: pts + %" PRItick, clock->offset);
#endif
}

sc_tick
sc_clock_to_system_time(struct sc_clock *clock, sc_tick stream) {
    assert(clock->range); // sc_clock_update() must have been called
    return stream + clock->offset;
}
