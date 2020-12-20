#include "icon.h"

#include <assert.h>
#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>

#include "config.h"
#include "command.h"
#include "compat.h"
#include "util/log.h"
#include "util/str_util.h"

#define SCRCPY_PORTABLE_ICON_FILENAME "icon.png"
#define SCRCPY_DEFAULT_ICON_PATH PREFIX \
    "/share/icons/hicolor/512x512/apps/scrcpy.png"

static char *
get_icon_path(void) {
#ifdef __WINDOWS__
    const wchar_t *icon_path_env = _wgetenv(L"SCRCPY_ICON_PATH");
#else
    const char *icon_path_env = getenv("SCRCPY_ICON_PATH");
#endif
    if (icon_path_env) {
        // if the envvar is set, use it
#ifdef __WINDOWS__
        char *icon_path = utf8_from_wide_char(icon_path_env);
#else
        char *icon_path = SDL_strdup(icon_path_env);
#endif
        if (!icon_path) {
            LOGE("Could not allocate memory");
            return NULL;
        }
        LOGD("Using SCRCPY_ICON_PATH: %s", icon_path);
        return icon_path;
    }

#ifndef PORTABLE
    char *icon_path = SDL_strdup(SCRCPY_DEFAULT_ICON_PATH);
    if (!icon_path) {
        LOGE("Could not allocate memory");
        return NULL;
    }
#else
    char *icon_path = get_local_file_path(SCRCPY_PORTABLE_ICON_FILENAME);
    if (!icon_path) {
        LOGE("Could not get icon path");
        return NULL;
    }
#endif

    return icon_path;
}

static AVFrame *
decode_image(const char *path) {
    AVFrame *result = NULL;

    AVFormatContext *ctx = avformat_alloc_context();
    if (!ctx) {
        LOGE("Could not allocate image decoder context");
        return NULL;
    }

    if (avformat_open_input(&ctx, path, NULL, NULL) < 0) {
        LOGE("Could not open image codec");
        goto free_ctx;
    }

    if (avformat_find_stream_info(ctx, NULL) < 0) {
        LOGE("Could not find image stream info");
        goto close_input;
    }

    int stream = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream < 0 ) {
        LOGE("Could not find best image stream");
        goto close_input;
    }

#ifdef SCRCPY_LAVF_HAS_NEW_CODEC_PARAMS_API
    AVCodecParameters *params = ctx->streams[stream]->codecpar;

    AVCodec *codec = avcodec_find_decoder(params->codec_id);
    if (!codec) {
        LOGE("Could not find image decoder");
        goto close_input;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_ctx, params) < 0) {
        LOGE("Could not fill codec context");
        goto free_codec_ctx;
    }
#else
    AVCodecContext *codec_ctx = ctx->streams[stream]->codec;

    AVCodec *codec = avcodec_find_decoder(codec_ctx->codec_id);
    if (!codec) {
        LOGE("Could not find image decoder");
        goto close_input;
    }
#endif

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        LOGE("Could not open image codec");
        goto free_codec_ctx;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        LOGE("Could not allocate frame");
        goto close_codec;
    }

    AVPacket packet;
    av_init_packet(&packet);

    if (av_read_frame(ctx, &packet) < 0) {
        LOGE("Could not read frame");
        av_frame_free(&frame);
        goto close_input;
    }

#ifdef SCRCPY_LAVF_HAS_NEW_ENCODING_DECODING_API
    int ret;
    if ((ret = avcodec_send_packet(codec_ctx, &packet)) < 0) {
        LOGE("Could not send icon packet: %d", ret);
        av_frame_free(&frame);
        goto close_input;
    }

    if ((ret = avcodec_receive_frame(codec_ctx, frame)) != 0) {
        LOGE("Could not receive icon frame: %d", ret);
        av_frame_free(&frame);
        goto close_input;
    }

#else
    int got_picture;
    int len = avcodec_decode_video2(codec_ctx, frame, &got_picture, &packet);
    if (len < 0 || !got_picture) {
        LOGE("Could not decode icon: %d", len);
        av_frame_free(&frame);
        goto close_input;
    }
#endif

    av_packet_unref(&packet);

    result = frame;

close_codec:
    avcodec_close(codec_ctx);
free_codec_ctx:
#ifdef SCRCPY_LAVF_HAS_NEW_CODEC_PARAMS_API
    avcodec_free_context(&codec_ctx);
#endif
close_input:
    avformat_close_input(&ctx);
free_ctx:
    avformat_free_context(ctx);

    return result;
}

static SDL_PixelFormatEnum
to_sdl_pixel_format(enum AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_RGB24: return SDL_PIXELFORMAT_RGB24;
        case AV_PIX_FMT_BGR24: return SDL_PIXELFORMAT_BGR24;
        case AV_PIX_FMT_ARGB: return SDL_PIXELFORMAT_ARGB32;
        case AV_PIX_FMT_RGBA: return SDL_PIXELFORMAT_RGBA32;
        case AV_PIX_FMT_ABGR: return SDL_PIXELFORMAT_ABGR32;
        case AV_PIX_FMT_BGRA: return SDL_PIXELFORMAT_BGRA32;
        case AV_PIX_FMT_RGB565BE: return SDL_PIXELFORMAT_RGB565;
        case AV_PIX_FMT_RGB555BE: return SDL_PIXELFORMAT_RGB555;
        case AV_PIX_FMT_BGR565BE: return SDL_PIXELFORMAT_BGR565;
        case AV_PIX_FMT_BGR555BE: return SDL_PIXELFORMAT_BGR555;
        case AV_PIX_FMT_RGB444BE: return SDL_PIXELFORMAT_RGB444;
        case AV_PIX_FMT_BGR444BE: return SDL_PIXELFORMAT_BGR444;
        default: return SDL_PIXELFORMAT_UNKNOWN;
    }
}

SDL_Surface *
scrcpy_icon_load() {
    char *icon_path = get_icon_path();
    assert(icon_path);

    AVFrame *frame = decode_image(icon_path);
    SDL_free(icon_path);
    if (!frame) {
        return NULL;
    }

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(frame->format);
    if (!desc) {
        LOGE("Could not get icon format descriptor");
        goto error;
    }

    bool is_packed_rgb = desc->flags & AV_PIX_FMT_FLAG_RGB
                    && !(desc->flags & AV_PIX_FMT_FLAG_PLANAR);
    if (!is_packed_rgb) {
        LOGE("Could not load non-RGB icon");
        goto error;
    }

    SDL_PixelFormatEnum format = to_sdl_pixel_format(frame->format);
    if (format == SDL_PIXELFORMAT_UNKNOWN) {
        LOGE("Unsupported icon pixel format: %s (%d)", desc->name,
                                                       frame->format);
        goto error;
    }

    int bits_per_pixel = av_get_bits_per_pixel(desc);
    SDL_Surface *surface =
        SDL_CreateRGBSurfaceWithFormatFrom(frame->data[0],
                                           frame->width, frame->height,
                                           bits_per_pixel,
                                           frame->linesize[0],
                                           format);

    if (!surface) {
        LOGE("Could not create icon surface");
        goto error;
    }

    surface->userdata = frame; // frame owns the data

    return surface;

error:
    av_frame_free(&frame);
    return NULL;
}

void
scrcpy_icon_destroy(SDL_Surface *icon) {
    AVFrame *frame = icon->userdata;
    assert(frame);
    av_frame_free(&frame);
    SDL_FreeSurface(icon);
}
