#ifndef SC_COORDS
#define SC_COORDS

#include <stdint.h>

struct sc_size {
    uint16_t width;
    uint16_t height;
};

struct sc_point {
    int32_t x;
    int32_t y;
};

struct sc_position {
    // The video screen size may be different from the real device screen size,
    // so store to which size the absolute position apply, to scale it
    // accordingly.
    struct sc_size screen_size;
    struct sc_point point;
};

#endif
