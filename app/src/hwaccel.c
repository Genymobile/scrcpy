#include "hwaccel.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>

#ifdef SC_HAVE_D3D11
# include <d3d10.h>
#endif

#include "util/log.h"

#ifdef SC_HAVE_VAAPI
# define SC_HWACCEL_NAME "VA-API"
# define SC_HWACCEL_DEVICE_TYPE AV_HWDEVICE_TYPE_VAAPI
# define SC_HWACCEL_PIXEL_FORMAT AV_PIX_FMT_VAAPI
#elif defined(SC_HAVE_D3D11)
# define SC_HWACCEL_NAME "D3D11"
# define SC_HWACCEL_DEVICE_TYPE AV_HWDEVICE_TYPE_D3D11VA
# define SC_HWACCEL_PIXEL_FORMAT AV_PIX_FMT_D3D11
#elif defined(SC_HAVE_VIDEOTOOLBOX)
# define SC_HWACCEL_NAME "VideoToolbox"
# define SC_HWACCEL_DEVICE_TYPE AV_HWDEVICE_TYPE_VIDEOTOOLBOX
# define SC_HWACCEL_PIXEL_FORMAT AV_PIX_FMT_VIDEOTOOLBOX
#else
# error "No hardware decoding backend selected"
#endif

bool
sc_hwaccel_set_hints(void) {
#ifdef SC_HAVE_D3D11
    // The decoder and renderer use the device from different threads. This
    // prevents SDL from creating it with D3D11_CREATE_DEVICE_SINGLETHREADED.
    return SDL_SetHintWithPriority(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1",
                                   SDL_HINT_OVERRIDE);
#else
    return true;
#endif
}

static enum AVPixelFormat
sc_hwaccel_get_format(AVCodecContext *ctx,
                      const enum AVPixelFormat *formats) {
    // On hwaccel initialization failure, FFmpeg calls get_format() again with
    // the failing format removed. Remember that we already selected it, to
    // report an accurate reason on the second call.
    bool retry = ctx->opaque != NULL;

    for (const enum AVPixelFormat *format = formats;
         *format != AV_PIX_FMT_NONE; ++format) {
        if (*format == SC_HWACCEL_PIXEL_FORMAT) {
            ctx->opaque = (void *) (uintptr_t) 1;
            return *format;
        }
    }

    for (const enum AVPixelFormat *format = formats;
         *format != AV_PIX_FMT_NONE; ++format) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*format);
        if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
            if (retry) {
                LOGI(SC_HWACCEL_NAME " unavailable: hardware decoder "
                     "initialization failed; using software decoding");
            } else {
                LOGI(SC_HWACCEL_NAME " unavailable: decoder does not offer "
                     "the required pixel format; using software decoding");
            }
            return *format;
        }
    }

    return AV_PIX_FMT_NONE;
}

#ifdef SC_HAVE_D3D11
static bool
sc_d3d11_enable_multithread_protection(ID3D11Device *device) {
    // Avoid depending on an external IID symbol when linking with MinGW.
    static const GUID iid_multithread = {
        0x9b7e4e00, 0x342c, 0x4106,
        {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0},
    };

    ID3D10Multithread *multithread = NULL;
    HRESULT hr = ID3D11Device_QueryInterface(device, &iid_multithread,
                                              (void **) &multithread);
    if (FAILED(hr)) {
        return false;
    }

    ID3D10Multithread_SetMultithreadProtected(multithread, TRUE);
    ID3D10Multithread_Release(multithread);
    return true;
}
#endif

void
sc_hwaccel_init(struct sc_hwaccel *hwaccel, SDL_Renderer *renderer,
                bool enabled) {
    memset(hwaccel, 0, sizeof(*hwaccel));
    hwaccel->unavailable_reason = "unknown initialization failure";

    if (!enabled) {
        // Do not probe the platform API
        hwaccel->unavailable_reason = "hardware decoding is disabled";
        return;
    }

#ifdef SC_HAVE_VAAPI
    if (!sc_vaapi_init(&hwaccel->vaapi, renderer,
                       &hwaccel->unavailable_reason)) {
        return;
    }
#elif defined(SC_HAVE_D3D11)
    const char *renderer_name = SDL_GetRendererName(renderer);
    if (!renderer_name || strcmp(renderer_name, "direct3d11")) {
        hwaccel->unavailable_reason =
            "the selected SDL renderer is not Direct3D 11";
        return;
    }

    SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
    hwaccel->device = SDL_GetPointerProperty(
        props, SDL_PROP_RENDERER_D3D11_DEVICE_POINTER, NULL);
    if (!hwaccel->device) {
        hwaccel->unavailable_reason =
            "SDL did not expose its Direct3D 11 device";
        return;
    }

    ID3D11Device_GetImmediateContext(hwaccel->device,
                                     &hwaccel->device_context);
    if (!hwaccel->device_context) {
        hwaccel->unavailable_reason =
            "the Direct3D 11 immediate context is unavailable";
        return;
    }

    // FFmpeg decodes on its worker thread while SDL renders on the main
    // thread, and both use this immediate context.
    if (!sc_d3d11_enable_multithread_protection(hwaccel->device)) {
        hwaccel->unavailable_reason =
            "the Direct3D 11 device does not support multithread protection";
        ID3D11DeviceContext_Release(hwaccel->device_context);
        hwaccel->device_context = NULL;
        return;
    }
#else
    const char *renderer_name = SDL_GetRendererName(renderer);
    if (!renderer_name || strcmp(renderer_name, "metal")) {
        hwaccel->unavailable_reason =
            "the selected SDL renderer is not Metal";
        return;
    }
#endif

    hwaccel->available = true;
    hwaccel->unavailable_reason = NULL;
}

void
sc_hwaccel_reset(struct sc_hwaccel *hwaccel) {
#ifdef SC_HAVE_VAAPI
    sc_vaapi_reset(&hwaccel->vaapi);
#else
    (void) hwaccel;
#endif
}

void
sc_hwaccel_destroy(struct sc_hwaccel *hwaccel) {
#ifdef SC_HAVE_VAAPI
    sc_vaapi_destroy(&hwaccel->vaapi);
#elif defined(SC_HAVE_D3D11)
    if (hwaccel->device_context) {
        ID3D11DeviceContext_Release(hwaccel->device_context);
    }
#else
    (void) hwaccel;
#endif
}

#ifdef SC_HAVE_D3D11
// FFmpeg allocates the decoder surfaces as a single D3D11 texture array, which
// the driver specification limits to 64 slices (MAX_ARRAY_SIZE in
// libavutil/hwcontext_d3d11va.c). The decoder keeps some of them for itself:
// ff_dxva2_common_frame_params() in libavcodec/dxva2.c takes 1 work surface
// plus the possible references (16 for H.264/H.265, 8 for VP9/AV1), then
// ff_decode_get_hw_frames_ctx() in libavcodec/decode.c adds 3 to guarantee 4
// work surfaces. extra_hw_frames may claim the rest.
# define SC_HWACCEL_MAX_BUFFERED_FRAMES (64 - 20)
#endif

bool
sc_hwaccel_configure_decoder(struct sc_hwaccel *hwaccel, AVCodecContext *ctx,
                             int buffered_frames) {
    assert(hwaccel->available);
    assert(ctx->codec_type == AVMEDIA_TYPE_VIDEO);
#ifndef SC_HAVE_D3D11
    (void) hwaccel;
#endif

    int ret;
#ifdef SC_HAVE_D3D11
    ctx->hw_device_ctx = av_hwdevice_ctx_alloc(SC_HWACCEL_DEVICE_TYPE);
    if (!ctx->hw_device_ctx) {
        LOG_OOM();
        return false;
    }

    AVHWDeviceContext *device_ctx =
        (AVHWDeviceContext *) ctx->hw_device_ctx->data;
    AVD3D11VADeviceContext *d3d11 =
        (AVD3D11VADeviceContext *) device_ctx->hwctx;
    d3d11->device = hwaccel->device;
    ID3D11Device_AddRef(d3d11->device);
    d3d11->device_context = hwaccel->device_context;
    ID3D11DeviceContext_AddRef(d3d11->device_context);
    ret = av_hwdevice_ctx_init(ctx->hw_device_ctx);
#else
    ret = av_hwdevice_ctx_create(&ctx->hw_device_ctx,
                                 SC_HWACCEL_DEVICE_TYPE, NULL, NULL, 0);
#endif
    if (ret < 0) {
        char err[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err, sizeof(err));
        LOGI(SC_HWACCEL_NAME " unavailable: could not create device: %s; "
             "using software decoding", err);
        av_buffer_unref(&ctx->hw_device_ctx);
        return false;
    }

#ifdef SC_HWACCEL_MAX_BUFFERED_FRAMES
    if (buffered_frames > SC_HWACCEL_MAX_BUFFERED_FRAMES) {
        LOGW("The video buffer is too large for " SC_HWACCEL_NAME " decoding: "
             "only %d delayed frames fit in the surface pool. Decrease "
             "--video-buffer or use --no-hardware-decoding.",
             SC_HWACCEL_MAX_BUFFERED_FRAMES);
        buffered_frames = SC_HWACCEL_MAX_BUFFERED_FRAMES;
    }
    ctx->extra_hw_frames = buffered_frames;
#else
    (void) buffered_frames;
#endif

    // Some backends do not expose plain Baseline (only Constrained Baseline).
    // Hardware supporting Main/High can still decode Baseline.
    ctx->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH;
    ctx->get_format = sc_hwaccel_get_format;
    return true;
}

bool
sc_hwaccel_is_frame(const AVFrame *frame) {
    return frame->format == SC_HWACCEL_PIXEL_FORMAT;
}

static enum AVPixelFormat
sc_hwaccel_get_software_format(const AVFrame *frame) {
    if (!frame->hw_frames_ctx) {
        return AV_PIX_FMT_NONE;
    }

    const AVHWFramesContext *frames =
        (const AVHWFramesContext *) frame->hw_frames_ctx->data;
    return frames->sw_format;
}

SDL_PixelFormat
sc_hwaccel_get_texture_format(const AVFrame *frame) {
    enum AVPixelFormat format = sc_hwaccel_get_software_format(frame);
    switch (format) {
        case AV_PIX_FMT_NV12:
            return SDL_PIXELFORMAT_NV12;
        case AV_PIX_FMT_P010:
            return SDL_PIXELFORMAT_P010;
        default:
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static void
sc_hwaccel_get_texture_size(const AVFrame *frame, int *width, int *height) {
#if defined(SC_HAVE_D3D11) || defined(SC_HAVE_VIDEOTOOLBOX)
    const AVHWFramesContext *frames =
        (const AVHWFramesContext *) frame->hw_frames_ctx->data;
    *width = frames->width;
    *height = frames->height;
#else
    *width = frame->width;
    *height = frame->height;
#endif
}

bool
sc_hwaccel_needs_new_texture(SDL_Texture *texture, const AVFrame *frame) {
#ifdef SC_HAVE_VIDEOTOOLBOX
    (void) texture;
    (void) frame;
    return true;
#elif defined(SC_HAVE_D3D11)
    int expected_width;
    int expected_height;
    sc_hwaccel_get_texture_size(frame, &expected_width, &expected_height);

    float width;
    float height;
    return !SDL_GetTextureSize(texture, &width, &height)
        || width != expected_width
        || height != expected_height;
#else
    (void) texture;
    (void) frame;
    return false;
#endif
}

SDL_Texture *
sc_hwaccel_create_texture(struct sc_hwaccel *hwaccel,
                          SDL_Renderer *renderer, const AVFrame *frame,
                          SDL_Colorspace colorspace) {
    (void) hwaccel;
    SDL_PixelFormat format = sc_hwaccel_get_texture_format(frame);
#ifdef SC_HAVE_VAAPI
    // The EGL import path currently supports only 8-bit NV12.
    if (format != SDL_PIXELFORMAT_NV12) {
        LOGW("VA-API zero-copy unavailable: unsupported software pixel "
             "format %d", sc_hwaccel_get_software_format(frame));
        return NULL;
    }
    if (!sc_vaapi_set_nv12_rg_shader(true)) {
        LOGE("Could not select SDL's DMA-BUF NV12 shader");
        return NULL;
    }
#else
    if (format == SDL_PIXELFORMAT_UNKNOWN) {
        LOGW(SC_HWACCEL_NAME " rendering unavailable: unsupported software "
             "pixel format %d", sc_hwaccel_get_software_format(frame));
        return NULL;
    }
#endif

    int width;
    int height;
    sc_hwaccel_get_texture_size(frame, &width, &height);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (!props) {
        return NULL;
    }

    bool ok =
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              format);
#ifdef SC_HAVE_VAAPI
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                                SDL_TEXTUREACCESS_STREAMING);
#else
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                                SDL_TEXTUREACCESS_STATIC);
#endif
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,
                                width);
    ok &= SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,
                                height);
    ok &= SDL_SetNumberProperty(props,
                                SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,
                                colorspace);
#ifdef SC_HAVE_VIDEOTOOLBOX
    if (!frame->data[3]) {
        ok = false;
    } else {
        ok &= SDL_SetPointerProperty(
            props, SDL_PROP_TEXTURE_CREATE_METAL_PIXELBUFFER_POINTER,
            frame->data[3]);
    }
#endif

    SDL_Texture *texture = NULL;
    if (ok) {
        texture = SDL_CreateTextureWithProperties(renderer, props);
    } else {
        LOGE("Could not set hardware texture properties");
    }
    SDL_DestroyProperties(props);

    if (!texture) {
        LOGD("Could not create " SC_HWACCEL_NAME " texture: %s",
             SDL_GetError());
    }
    return texture;
}

bool
sc_hwaccel_update_texture(struct sc_hwaccel *hwaccel,
                          SDL_Renderer *renderer, SDL_Texture *texture,
                          const AVFrame *frame) {
    bool ok;
#ifdef SC_HAVE_VAAPI
    ok = sc_vaapi_set_frame(&hwaccel->vaapi, renderer, texture, frame);
#elif defined(SC_HAVE_D3D11)
    SDL_PropertiesID props = SDL_GetTextureProperties(texture);
    ID3D11Resource *dst = SDL_GetPointerProperty(
        props, SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER, NULL);
    if (!dst) {
        LOGW("D3D11 rendering unavailable: SDL did not expose its texture");
        return false;
    }

    ID3D11Resource *src = (ID3D11Resource *) frame->data[0];
    if (!src) {
        LOGW("D3D11 rendering unavailable: decoded surface is missing");
        return false;
    }

    if (!SDL_FlushRenderer(renderer)) {
        LOGD("Could not flush renderer before D3D11 surface copy: %s",
             SDL_GetError());
    }
    UINT slice = (UINT) (uintptr_t) frame->data[1];
    ID3D11DeviceContext_CopySubresourceRegion(hwaccel->device_context,
                                              dst, 0, 0, 0, 0,
                                              src, slice, NULL);
    ok = true;
#else
    (void) hwaccel;
    (void) renderer;
    (void) texture;
    (void) frame;
    ok = true; // The SDL texture directly wraps the CVPixelBuffer.
#endif

    if (ok && !hwaccel->use_logged) {
#ifdef SC_HAVE_VAAPI
        LOGI("Video decoding: VA-API with zero-copy DMA-BUF rendering");
#elif defined(SC_HAVE_D3D11)
        LOGI("Video decoding: D3D11 with GPU surface copies");
#else
        LOGI("Video decoding: VideoToolbox with zero-copy Metal rendering");
#endif
        hwaccel->use_logged = true;
    }
    return ok;
}

bool
sc_hwaccel_prepare_software(struct sc_hwaccel *hwaccel) {
#ifdef SC_HAVE_VAAPI
    (void) hwaccel;
    return sc_vaapi_set_nv12_rg_shader(false);
#else
    (void) hwaccel;
    return true;
#endif
}

bool
sc_hwaccel_disable_rendering(struct sc_hwaccel *hwaccel) {
    hwaccel->rendering_failed = true;
    LOGW(SC_HWACCEL_NAME " rendering failed; transferring hardware frames "
         "to system memory");
    if (!sc_hwaccel_prepare_software(hwaccel)) {
        LOGE("Could not restore SDL's software texture configuration");
        return false;
    }
    return true;
}
