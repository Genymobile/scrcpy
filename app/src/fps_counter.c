#include "fps_counter.h"

#include <SDL2/SDL_timer.h>

#include "log.h"

void
fps_counter_init(struct fps_counter *counter) {
    counter->started = false;
    // no need to initialize the other fields, they are meaningful only when
    // started is true
}

void
fps_counter_start(struct fps_counter *counter) {
    counter->started = true;
    counter->slice_start = SDL_GetTicks();
    counter->nr_rendered = 0;
    counter->nr_skipped = 0;
}

void
fps_counter_stop(struct fps_counter *counter) {
    counter->started = false;
}

static void
display_fps(struct fps_counter *counter) {
    if (counter->nr_skipped) {
        LOGI("%d fps (+%d frames skipped)", counter->nr_rendered,
                                            counter->nr_skipped);
    } else {
        LOGI("%d fps", counter->nr_rendered);
    }
}

static void
check_expired(struct fps_counter *counter) {
    uint32_t now = SDL_GetTicks();
    if (now - counter->slice_start >= 1000) {
        display_fps(counter);
        // add a multiple of one second
        uint32_t elapsed_slices = (now - counter->slice_start) / 1000;
        counter->slice_start += 1000 * elapsed_slices;
        counter->nr_rendered = 0;
        counter->nr_skipped = 0;
    }
}

void
fps_counter_add_rendered_frame(struct fps_counter *counter) {
    check_expired(counter);
    ++counter->nr_rendered;
}

void
fps_counter_add_skipped_frame(struct fps_counter *counter) {
    check_expired(counter);
    ++counter->nr_skipped;
}
