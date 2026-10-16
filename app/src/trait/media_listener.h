#ifndef SC_MEDIA_LISTENER_H
#define SC_MEDIA_LISTENER_H

#include "common.h"

#include <stdint.h>

#include "device_msg.h"

/**
 * Media listener trait.
 *
 * Component able to receive the media session updates of the device should
 * implement this trait.
 *
 * The callbacks are called from the receiver thread.
 */
struct sc_media_listener {
    const struct sc_media_listener_ops *ops;
};

struct sc_media_listener_ops {
    /**
     * Called when the state or the metadata of the media session changes.
     *
     * When there is no media session, the state is stopped, the capabilities
     * are empty and the strings are NULL.
     *
     * The listener takes ownership of the strings of the update.
     */
    void
    (*on_update)(struct sc_media_listener *listener,
                 struct sc_media_update *update);
};

#endif
