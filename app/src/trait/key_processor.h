#ifndef SC_KEY_PROCESSOR_H
#define SC_KEY_PROCESSOR_H

#include "common.h"

#include <assert.h>
#include <stdbool.h>

#include <SDL2/SDL_events.h>

/**
 * Key processor trait.
 *
 * Component able to process and inject keys should implement this trait.
 */
struct sc_key_processor {
    const struct sc_key_processor_ops *ops;
};

struct sc_key_processor_ops {

    /**
     * Process the keyboard event
     *
     * The flag `device_clipboard_set` indicates that the input manager sent a
     * control message to synchronize the device clipboard as a result of this
     * key event.
     */
    void
    (*process_key)(struct sc_key_processor *kp, const SDL_KeyboardEvent *event,
                   bool device_clipboard_set);

    void
    (*process_text)(struct sc_key_processor *kp,
                    const SDL_TextInputEvent *event);
};

#endif
