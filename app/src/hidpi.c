#include "hidpi.h"

void hidpi_get_scale(struct screen *screen, struct hidpi_scale *scale) {
    SDL_GL_GetDrawableSize(screen->window, &scale->horizontal.num, &scale->vertical.num);
    SDL_GetWindowSize(screen->window, &scale->horizontal.div, &scale->vertical.div);
}

void hidpi_unscale_coordinates(struct hidpi_scale *scale, Sint32 *x, Sint32 *y) {
    // to unscale, we devide by the ratio (so num and div are reversed)
    if (scale->horizontal.num) {
        *x = ((Sint64) *x) * scale->horizontal.div / scale->horizontal.num;
    }
    if (scale->vertical.num) {
        *y = ((Sint64) *y) * scale->vertical.div / scale->vertical.num;
    }
}
