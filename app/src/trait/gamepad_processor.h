#ifndef SC_GAMEPAD_PROCESSOR_H
#define SC_GAMEPAD_PROCESSOR_H

#include "common.h"

#include "input_events.h"

/**
 * Gamepad processor trait.
 *
 * Component able to handle gamepads devices and inject buttons and axis events.
 */
struct sc_gamepad_processor {
    const struct sc_gamepad_processor_ops *ops;
};

struct sc_gamepad_processor_ops {

    /**
     * Process a gamepad device added event
     *
     * This function is mandatory.
     */
    void
    (*process_gamepad_added)(struct sc_gamepad_processor *gp,
                             const struct sc_gamepad_device_event *event);

    /**
     * Process a gamepad device removed event
     *
     * This function is mandatory.
     */
    void
    (*process_gamepad_removed)(struct sc_gamepad_processor *gp,
                               const struct sc_gamepad_device_event *event);

    /**
     * Process a gamepad axis event
     *
     * This function is mandatory.
     */
    void
    (*process_gamepad_axis)(struct sc_gamepad_processor *gp,
                            const struct sc_gamepad_axis_event *event);

    /**
     * Process a gamepad button event
     *
     * This function is mandatory.
     */
    void
    (*process_gamepad_button)(struct sc_gamepad_processor *gp,
                              const struct sc_gamepad_button_event *event);
};

#endif
