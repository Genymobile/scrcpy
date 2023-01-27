#include "rand.h"

#include <stdlib.h>

#include "tick.h"

void sc_rand_init(struct sc_rand *rand) {
    sc_tick seed = sc_tick_now(); // microsecond precision
    rand->xsubi[0] = (seed >> 32) & 0xFFFF;
    rand->xsubi[1] = (seed >> 16) & 0xFFFF;
    rand->xsubi[2] = seed & 0xFFFF;
}

uint32_t sc_rand_u32(struct sc_rand *rand) {
    // jrand returns a value in range [-2^31, 2^31]
    // conversion from signed to unsigned is well-defined to wrap-around
    return jrand48(rand->xsubi);
}

uint64_t sc_rand_u64(struct sc_rand *rand) {
    uint32_t msb = sc_rand_u32(rand);
    uint32_t lsb = sc_rand_u32(rand);
    return ((uint64_t) msb << 32) | lsb;
}
