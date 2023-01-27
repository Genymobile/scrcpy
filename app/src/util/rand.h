#ifndef SC_RAND_H
#define SC_RAND_H

#include "common.h"

#include <inttypes.h>

struct sc_rand {
    unsigned short xsubi[3];
};

void sc_rand_init(struct sc_rand *rand);
uint32_t sc_rand_u32(struct sc_rand *rand);
uint64_t sc_rand_u64(struct sc_rand *rand);

#endif
