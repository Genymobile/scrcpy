#ifndef SC_TICK_H
#define SC_TICK_H

#include "common.h"

#include <stdint.h>

typedef int64_t sc_tick;
#define PRItick PRIi64
#define SC_TICK_FREQ 1000000 // microsecond

// To be adapted if SC_TICK_FREQ changes
#define SC_TICK_TO_NS(tick) ((tick) * 1000)
#define SC_TICK_TO_US(tick) (tick)
#define SC_TICK_TO_MS(tick) ((tick) / 1000)
#define SC_TICK_TO_SEC(tick) ((tick) / 1000000)
#define SC_TICK_FROM_NS(ns) ((ns) / 1000)
#define SC_TICK_FROM_US(us) (us)
#define SC_TICK_FROM_MS(ms) ((ms) * 1000)
#define SC_TICK_FROM_SEC(sec) ((sec) * 1000000)

sc_tick
sc_tick_now(void);

#endif
