#include "tick.h"

#include <SDL2/SDL_timer.h>

sc_tick
sc_tick_now(void) {
    // SDL_GetTicks() resolution is in milliseconds, but sc_tick are expressed
    // in microseconds to avoid loosing precision on PTS.
    //
    // SDL_GetPerformanceCounter()/SDL_GetPerformanceFrequency() could be used,
    // but:
    //  - the conversions (to avoid overflow) are not zero-cost, since the
    //    frequency is not known at compile time;
    //  - in practice, we don't need more precision for now.
    return (sc_tick) SDL_GetTicks() * 1000;
}
