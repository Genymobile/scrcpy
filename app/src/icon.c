#include "icon.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <SDL2/SDL.h>

#include "config.h"
#include "util/env.h"
#ifdef PORTABLE
# include "util/file.h"
#endif
#include "util/log.h"

#define SCRCPY_PORTABLE_ICON_FILENAME "icon.png"
#define SCRCPY_DEFAULT_ICON_PATH \
    PREFIX "/share/icons/hicolor/256x256/apps/scrcpy.png"

static char *
get_icon_path(void) {
    char *icon_path = sc_get_env("SCRCPY_ICON_PATH");
    if (icon_path) {
        // if the envvar is set, use it
        LOGD("Using SCRCPY_ICON_PATH: %s", icon_path);
        return icon_path;
    }

#ifndef PORTABLE
    LOGD("Using icon: " SCRCPY_DEFAULT_ICON_PATH);
    icon_path = strdup(SCRCPY_DEFAULT_ICON_PATH);
    if (!icon_path) {
        LOG_OOM();
        return NULL;
    }
#else
    icon_path = sc_file_get_local_path(SCRCPY_PORTABLE_ICON_FILENAME);
    if (!icon_path) {
        LOGE("Could not get icon path");
        return NULL;
    }
    LOGD("Using icon (portable): %s", icon_path);
#endif

    return icon_path;
}

static AVFrame *
decode_image(const char *path) {
    AVFrame *result = NULL;

    AVFormatContext *ctx = avformat_alloc_context();
    if (!ctx) {
        LOG_OOM();
        return NULL;
    }

    if (avformat_open_input(&ctx, path, NULL, NULL) < 0) {
        LOGE("Could not open icon image: %s", path);
        goto free_ctx;
    }

    if (avformat_find_stream_info(ctx, NULL) < 0) {
        LOGE("Could not find image stream info");
        goto close_input;
    }


// In ffmpeg/doc/APIchanges:
// 2021-04-27 - 46dac8cf3d - lavf 59.0.100 - avformat.h
//   av_find_best_stream now uses a const AVCodec ** parameter
//   for the returned decoder.
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(59, 0, 100)
    const AVCodec *codec;
#else
    AVCodec *codec;
#endif

    int stream =
        av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream < 0 ) {
        LOGE("Could not find best image stream");
        goto close_input;
    }

    AVCodecParameters *params = ctx->streams[stream]->codecpar;

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        LOG_OOM();
        goto close_input;
    }

    if (avcodec_parameters_to_context(codec_ctx, params) < 0) {
        LOGE("Could not fill codec context");
        goto free_codec_ctx;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        LOGE("Could not open image codec");
        goto free_codec_ctx;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        LOG_OOM();
        goto free_codec_ctx;
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOG_OOM();
        av_frame_free(&frame);
        goto free_codec_ctx;
    }

    if (av_read_frame(ctx, packet) < 0) {
        LOGE("Could not read frame");
        av_packet_free(&packet);
        av_frame_free(&frame);
        goto free_codec_ctx;
    }

    int ret;
    if ((ret = avcodec_send_packet(codec_ctx, packet)) < 0) {
        LOGE("Could not send icon packet: %d", ret);
        av_packet_free(&packet);
        av_frame_free(&frame);
        goto free_codec_ctx;
    }

    if ((ret = avcodec_receive_frame(codec_ctx, frame)) != 0) {
        LOGE("Could not receive icon frame: %d", ret);
        av_packet_free(&packet);
        av_frame_free(&frame);
        goto free_codec_ctx;
    }

    av_packet_free(&packet);

    result = frame;

free_codec_ctx:
    avcodec_free_context(&codec_ctx);
close_input:
    avformat_close_input(&ctx);
free_ctx:
    avformat_free_context(ctx);

    return result;
}

#if !SDL_VERSION_ATLEAST(2, 0, 10)
// SDL_PixelFormatEnum has been introduced in SDL 2.0.10. Use int for older SDL
// versions.
typedef int SDL_PixelFormatEnum;
#endif

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
#if SDL_VERSION_ATLEAST(2, 0, 12)
        case AV_PIX_FMT_BGR444BE: return SDL_PIXELFORMAT_BGR444;
#endif
        case AV_PIX_FMT_PAL8: return SDL_PIXELFORMAT_INDEX8;
        default: return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static SDL_Surface *
load_from_path(const char *path) {
    AVFrame *frame = decode_image(path);
    if (!frame) {
        return NULL;
    }

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(frame->format);
    if (!desc) {
        LOGE("Could not get icon format descriptor");
        goto error;
    }

    bool is_packed = !(desc->flags & AV_PIX_FMT_FLAG_PLANAR);
    if (!is_packed) {
        LOGE("Could not load non-packed icon");
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

    if (frame->format == AV_PIX_FMT_PAL8) {
        // Initialize the SDL palette
        uint8_t *data = frame->data[1];
        SDL_Color colors[256];
        for (int i = 0; i < 256; ++i) {
            SDL_Color *color = &colors[i];

            // The palette is transported in AVFrame.data[1], is 1024 bytes
            // long (256 4-byte entries) and is formatted the same as in
            // AV_PIX_FMT_RGB32 described above (i.e., it is also
            // endian-specific).
            // <https://ffmpeg.org/doxygen/4.1/pixfmt_8h.html#a9a8e335cf3be472042bc9f0cf80cd4c5>
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            color->a = data[i * 4];
            color->r = data[i * 4 + 1];
            color->g = data[i * 4 + 2];
            color->b = data[i * 4 + 3];
#else
            color->a = data[i * 4 + 3];
            color->r = data[i * 4 + 2];
            color->g = data[i * 4 + 1];
            color->b = data[i * 4];
#endif
        }

        SDL_Palette *palette = surface->format->palette;
        assert(palette);
        int ret = SDL_SetPaletteColors(palette, colors, 0, 256);
        if (ret) {
            LOGE("Could not set palette colors");
            SDL_FreeSurface(surface);
            goto error;
        }
    }

    surface->userdata = frame; // frame owns the data

    return surface;

error:
    av_frame_free(&frame);
    return NULL;
}

SDL_Surface *
scrcpy_icon_load(void) {
    char *icon_path = get_icon_path();
    if (!icon_path) {
        return NULL;
    }

    SDL_Surface *icon = load_from_path(icon_path);
    free(icon_path);
    return icon;
}

void
scrcpy_icon_destroy(SDL_Surface *icon) {
    AVFrame *frame = icon->userdata;
    assert(frame);
    av_frame_free(&frame);
    SDL_FreeSurface(icon);
}
