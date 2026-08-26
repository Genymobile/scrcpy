#include "hwdec.h"

#include <assert.h>

#include <libavutil/error.h>
#include <libavutil/pixdesc.h>

#ifdef HAVE_D3D11VA
// Define the GUIDs (IID_ID3D10Multithread)
# define INITGUID
# define COBJMACROS
# include <d3d11.h>
# include <libavutil/hwcontext_d3d11va.h>
#endif

#include "util/log.h"

#ifdef HAVE_D3D11VA
static bool
sc_hwdec_init_d3d11va(struct sc_hwdec *hwdec, SDL_Renderer *renderer) {
    assert(renderer);

    // The properties are owned by the renderer
    SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
    ID3D11Device *device =
        SDL_GetPointerProperty(props, SDL_PROP_RENDERER_D3D11_DEVICE_POINTER,
                               NULL);
    if (!device) {
        LOGE("D3D11VA: SDL did not expose its Direct3D 11 device");
        return false;
    }

    UINT flags = ID3D11Device_GetCreationFlags(device);
    if (flags & D3D11_CREATE_DEVICE_SINGLETHREADED) {
        LOGE("D3D11VA: the Direct3D 11 device is single-threaded");
        return false;
    }

    ID3D10Multithread *multithread = NULL;
    HRESULT hr = ID3D11Device_QueryInterface(device, &IID_ID3D10Multithread,
                                             (void **) &multithread);
    if (FAILED(hr)) {
        LOGE("D3D11VA: could not enable multithread protection");
        return false;
    }
    ID3D10Multithread_SetMultithreadProtected(multithread, TRUE);
    ID3D10Multithread_Release(multithread);

    AVBufferRef *hw_device_ctx =
        av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!hw_device_ctx) {
        LOG_OOM();
        return false;
    }

    // FFmpeg releases its reference when the device context is freed
    AVHWDeviceContext *device_ctx = (AVHWDeviceContext *) hw_device_ctx->data;
    AVD3D11VADeviceContext *hwctx = device_ctx->hwctx;
    hwctx->device = device;
    ID3D11Device_AddRef(device);

    int ret = av_hwdevice_ctx_init(hw_device_ctx);
    if (ret < 0) {
        LOGE("Could not create D3D11VA device: %s", av_err2str(ret));
        av_buffer_unref(&hw_device_ctx);
        return false;
    }

    hwdec->hw_device_ctx = hw_device_ctx;
    return true;
}
#endif

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
#ifdef HAVE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI: {
            int ret = av_hwdevice_ctx_create(&hwdec->hw_device_ctx, hw_type,
                                             NULL, NULL, 0);
            if (ret < 0) {
                LOGE("Could not create VA-API device: %s", av_err2str(ret));
                return false;
            }

            hwdec->name = "vaapi";
            break;
        }
#endif
#ifdef HAVE_D3D11VA
        case AV_HWDEVICE_TYPE_D3D11VA:
            if (!sc_hwdec_init_d3d11va(hwdec, renderer)) {
                return false;
            }

            hwdec->name = "d3d11va";
            break;
#endif
#ifdef HAVE_VIDEOTOOLBOX
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: {
            int ret = av_hwdevice_ctx_create(&hwdec->hw_device_ctx, hw_type,
                                             NULL, NULL, 0);
            if (ret < 0) {
                LOGE("Could not create VideoToolbox device: %s",
                     av_err2str(ret));
                return false;
            }

            hwdec->name = "videotoolbox";
            break;
        }
#endif
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
