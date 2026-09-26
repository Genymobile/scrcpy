#ifndef SC_HWDEC_H
#define SC_HWDEC_H

#include "common.h"

#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <SDL3/SDL.h>

struct sc_hwdec {
    const char *name; // must be statically allocated (e.g. a string literal)

    enum AVHWDeviceType hw_type;
    AVBufferRef *hw_device_ctx;
};

bool
sc_hwdec_init(struct sc_hwdec *hwdec, enum AVHWDeviceType hw_type,
              SDL_Renderer *renderer);

bool
sc_hwdec_configure(struct sc_hwdec *hwdec, AVCodecContext *ctx);

void
sc_hwdec_destroy(struct sc_hwdec *hwdec);

#endif
