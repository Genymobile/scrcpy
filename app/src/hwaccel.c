#include "hwaccel.h"

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <SDL3/SDL.h>

#include "util/log.h"

bool
sc_hwaccel_is_supported(void) {
#ifdef __APPLE__
    const char *driver = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
    // Hardware decoding uses CVPixelBuffer-backed Metal textures, which
    // only the Metal renderer supports. Default (NULL) means Metal on
    // macOS — only reject explicitly non-Metal backends.
    if (driver && strncmp(driver, "metal", 5)) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool
sc_hwaccel_setup(AVCodecContext *ctx) {
#ifdef __APPLE__
    assert(ctx);

    // Idempotent: already configured by a prior call
    if (ctx->hw_device_ctx) {
        return true;
    }

    AVBufferRef *hw_dev = NULL;
    if (av_hwdevice_ctx_create(&hw_dev, AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
                                NULL, NULL, 0) >= 0) {
        ctx->hw_device_ctx = av_buffer_ref(hw_dev);
        av_buffer_unref(&hw_dev);
        if (!ctx->hw_device_ctx) {
            LOG_OOM();
            return false;
        }
        LOGI("VideoToolbox hardware acceleration enabled");
        return true;
    }
    LOGW("VideoToolbox hardware acceleration unavailable");
    return false;
#else
    (void) ctx;
    return false;
#endif
}
