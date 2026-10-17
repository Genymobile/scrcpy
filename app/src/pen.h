#ifndef SC_PEN_H
#define SC_PEN_H

#include <stdbool.h>
#include <SDL3/SDL_events.h>

struct sc_pen {
    SDL_PenID which;
    bool down;
    float pressure;
    float x;
    float y;
};

struct sc_pen_sample {
    unsigned action; // Android DOWN=0, UP=1, MOVE=2
    float x;
    float y;
    float pressure;
};

bool
sc_pen_is_synthetic(const SDL_Event *event);

bool
sc_pen_process(struct sc_pen *pen, const SDL_Event *event,
               struct sc_pen_sample *sample);

#endif
