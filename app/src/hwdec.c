#include "hwdec.h"

#include <assert.h>

#include <libavutil/pixdesc.h>

#include "util/log.h"

bool
sc_hwdec_init(struct sc_hwdec *hwdec, enum AVHWDeviceType hw_type,
              bool hw_forced, SDL_Renderer *renderer) {
    (void) renderer; // only used by some hardware decoders

    hwdec->hw_type = hw_type;
    hwdec->hw_forced = hw_forced;
    switch (hw_type) {
        case AV_HWDEVICE_TYPE_NONE:
            hwdec->name = "software";
            hwdec->hw_device_ctx = NULL;
            break;
        default:
            LOGE("No decoder for hardware device type: %s",
                 av_hwdevice_get_type_name(hw_type));
            return false;
    }

    LOGI("Video decoding: %s", hwdec->name);
    return true;
}

static bool
sc_hwdec_is_supported_by_codec(const AVCodec *codec,
                               enum AVHWDeviceType hw_type) {
    for (int i = 0;; ++i) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
        if (!config) {
            return false;
        }

        bool device_ctx =
            config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX;
        if (device_ctx && config->device_type == hw_type) {
            return true;
        }
    }
}

static enum AVPixelFormat
sc_hwdec_get_format_forced(AVCodecContext *ctx,
                           const enum AVPixelFormat *formats) {
    enum AVPixelFormat format = avcodec_default_get_format(ctx, formats);
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(format);
    if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        struct sc_hwdec *hwdec = ctx->opaque;
        assert(hwdec);
        LOGE("%s cannot decode this stream", hwdec->name);
        return AV_PIX_FMT_NONE;
    }
    return format;
}

bool
sc_hwdec_configure(struct sc_hwdec *hwdec, AVCodecContext *ctx) {
    assert(ctx->codec_type == AVMEDIA_TYPE_VIDEO);
    assert(ctx->codec);
    assert(!ctx->hw_device_ctx);

    if (hwdec->hw_type == AV_HWDEVICE_TYPE_NONE) {
        // Software decoding, nothing to configure
        return true;
    }

    if (!sc_hwdec_is_supported_by_codec(ctx->codec, hwdec->hw_type)) {
        LOGW("%s is not supported by the %s decoder",
             hwdec->name, ctx->codec->name);
        return false;
    }

    assert(hwdec->hw_device_ctx);
    ctx->hw_device_ctx = av_buffer_ref(hwdec->hw_device_ctx);
    if (!ctx->hw_device_ctx) {
        LOG_OOM();
        return false;
    }

    // Some backends do not expose plain Baseline (only Constrained Baseline).
    // Hardware supporting Main/High can still decode Baseline.
    ctx->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH;

    if (hwdec->hw_forced) {
        // By default, FFmpeg falls back to a software format if the hardware
        // decoder cannot decode the stream: refuse it
        ctx->opaque = hwdec;
        ctx->get_format = sc_hwdec_get_format_forced;
    }

    return true;
}

void
sc_hwdec_destroy(struct sc_hwdec *hwdec) {
    if (hwdec->hw_device_ctx) {
        av_buffer_unref(&hwdec->hw_device_ctx);
    }
}
