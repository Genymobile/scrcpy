#ifndef HIDPI_H
#define HIDPI_H

#include "common.h"
#include "screen.h"

// rational number p/q
struct rational {
    int num;
    int div;
};

struct hidpi_scale {
    struct rational horizontal; // drawable.width / window.width
    struct rational vertical; // drawable.height / window.height
};

void hidpi_get_scale(struct screen *screen, struct hidpi_scale *hidpi_scale);

// mouse location need to be "unscaled" if hidpi is enabled
// <https://nlguillemot.wordpress.com/2016/12/11/high-dpi-rendering/>
void hidpi_unscale_coordinates(struct hidpi_scale *hidpi_scale, Sint32 *x, Sint32 *y);

#endif
