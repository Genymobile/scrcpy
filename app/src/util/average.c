#include "average.h"

#include <assert.h>

void
sc_average_init(struct sc_average *avg, unsigned range) {
    avg->range = range;
    avg->avg = 0;
    avg->count = 0;
}

void
sc_average_push(struct sc_average *avg, float value) {
    if (avg->count < avg->range) {
        ++avg->count;
    }

    assert(avg->count);
    avg->avg = ((avg->count - 1) * avg->avg + value) / avg->count;
}

float
sc_average_get(struct sc_average *avg) {
    assert(avg->count);
    return avg->avg;
}
