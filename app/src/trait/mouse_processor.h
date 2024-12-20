#ifndef SC_MOUSE_PROCESSOR_H
#define SC_MOUSE_PROCESSOR_H

#include "common.h"

#include <stdbool.h>

#include "input_events.h"

/**
 * Mouse processor trait.
 *
 * Component able to process and inject mouse events should implement this
 * trait.
 */
struct sc_mouse_processor {
    const struct sc_mouse_processor_ops *ops;

    /**
     * If set, the mouse processor works in relative mode (the absolute
     * position is irrelevant). In particular, it indicates that the mouse
     * pointer must be "captured" by the UI.
     */
    bool relative_mode;
};

struct sc_mouse_processor_ops {
    /**
     * Process a mouse motion event
     *
     * This function is mandatory.
     */
    void
    (*process_mouse_motion)(struct sc_mouse_processor *mp,
                            const struct sc_mouse_motion_event *event);

    /**
     * Process a mouse click event
     *
     * This function is mandatory.
     */
    void
    (*process_mouse_click)(struct sc_mouse_processor *mp,
                           const struct sc_mouse_click_event *event);

    /**
     * Process a mouse scroll event
     *
     * This function is optional.
     */
    void
    (*process_mouse_scroll)(struct sc_mouse_processor *mp,
                            const struct sc_mouse_scroll_event *event);

    /**
     * Process a touch event
     *
     * This function is optional.
     */
    void
    (*process_touch)(struct sc_mouse_processor *mp,
                     const struct sc_touch_event *event);
};

#endif
