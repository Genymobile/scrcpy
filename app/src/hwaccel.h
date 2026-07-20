#ifndef SC_HWACCEL_H
#define SC_HWACCEL_H

#include <stdbool.h>

struct AVCodecContext;

/**
 * Check whether the current renderer supports hardware acceleration.
 *
 * Currently requires a Metal renderer; returns false for
 * OpenGL or unknown renderers.
 */
bool
sc_hwaccel_is_supported(void);

/**
 * Configure AVCodecContext for hardware-accelerated decoding.
 *
 * Must be called after avcodec_alloc_context3() and before
 * avcodec_open2(). On macOS, configures ctx->hw_device_ctx
 * for VideoToolbox. On other platforms, returns false
 * immediately (no-op).
 */
bool
sc_hwaccel_setup(struct AVCodecContext *ctx);

#endif
