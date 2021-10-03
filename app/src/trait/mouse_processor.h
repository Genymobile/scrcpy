#ifndef SC_MOUSE_PROCESSOR_H
#define SC_MOUSE_PROCESSOR_H

#include "common.h"

#include <assert.h>
#include <stdbool.h>

#include <SDL2/SDL_events.h>

/**
 * Mouse processor trait.
 *
 * Component able to process and inject mouse events should implement this
 * trait.
 */
struct sc_mouse_processor {
    const struct sc_mouse_processor_ops *ops;
};

struct sc_mouse_processor_ops {
    void
    (*process_mouse_motion)(struct sc_mouse_processor *mp,
                            const SDL_MouseMotionEvent *event);

    void
    (*process_touch)(struct sc_mouse_processor *mp,
                     const SDL_TouchFingerEvent *event);

    void
    (*process_mouse_button)(struct sc_mouse_processor *mp,
                            const SDL_MouseButtonEvent *event);

    void
    (*process_mouse_wheel)(struct sc_mouse_processor *mp,
                           const SDL_MouseWheelEvent *event);
};

#endif
