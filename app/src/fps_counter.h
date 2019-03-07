#ifndef FPSCOUNTER_H
#define FPSCOUNTER_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

struct fps_counter {
    bool started;
    uint32_t slice_start; // initialized by SDL_GetTicks()
    int nr_rendered;
#ifdef SKIP_FRAMES
    int nr_skipped;
#endif
};

void
fps_counter_init(struct fps_counter *counter);

void
fps_counter_start(struct fps_counter *counter);

void
fps_counter_stop(struct fps_counter *counter);

void
fps_counter_add_rendered_frame(struct fps_counter *counter);

#ifdef SKIP_FRAMES
void
fps_counter_add_skipped_frame(struct fps_counter *counter);
#endif

#endif
