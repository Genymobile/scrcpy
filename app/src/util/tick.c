#include "tick.h"

#include <SDL2/SDL_timer.h>

sc_tick
sc_tick_now(void) {
    // SDL_GetTicks() resolution is in milliseconds, but sc_tick are expressed
    // in microseconds to store PTS without precision loss.
    //
    // As an alternative, SDL_GetPerformanceCounter() and
    // SDL_GetPerformanceFrequency() could be used, but:
    //  - the conversions (avoiding overflow) are expansive, since the
    //    frequency is not known at compile time;
    //  - in practice, we don't need more precision for now.
    return (sc_tick) SDL_GetTicks() * 1000;
}
