#ifndef SC_AVERAGE
#define SC_AVERAGE

#include "common.h"

struct sc_average {
    // Current average value
    float avg;

    // Target range, to update the average as follow:
    //     avg = ((range - 1) * avg + new_value) / range
    unsigned range;

    // Number of values pushed when less than range (count <= range).
    // The purpose is to handle the first (range - 1) values properly.
    unsigned count;
};

void
sc_average_init(struct sc_average *avg, unsigned range);

/**
 * Push a new value to update the "rolling" average
 */
void
sc_average_push(struct sc_average *avg, float value);

/**
 * Get the current average value
 *
 * It is an error to call this function if sc_average_push() has not been
 * called at least once.
 */
float
sc_average_get(struct sc_average *avg);

#endif
