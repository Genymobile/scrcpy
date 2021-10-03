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
    void
    (*process_key)(struct sc_key_processor *kp, const SDL_KeyboardEvent *event);

    void
    (*process_text)(struct sc_key_processor *kp,
                    const SDL_TextInputEvent *event);
};

#endif
