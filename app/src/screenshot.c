#include "screenshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>

#include "util/log.h"

// Copy a saved PNG file onto the system clipboard as an image so it can be
// pasted directly into other apps.
static void
copy_png_to_clipboard(const char *filename) {
#ifdef __APPLE__
    // The AppleScript type code «class PNGf» contains the guillemets «»
    // which are U+00AB / U+00BB — encoded as UTF-8 below.
    char cmd[1024];
    int ret = snprintf(cmd, sizeof(cmd),
        "osascript -e 'set the clipboard to "
        "(read (POSIX file \"%s\") as \xc2\xab""class PNGf\xc2\xbb)' "
        ">/dev/null 2>&1 &",
        filename);
    if (ret <= 0 || (size_t) ret >= sizeof(cmd)) {
        LOGW("Could not build clipboard command");
        return;
    }
    int rc = system(cmd);
    if (rc != 0) {
        LOGW("Could not copy screenshot to clipboard (rc=%d)", rc);
    } else {
        LOGI("Screenshot copied to clipboard");
    }
#else
    // On Linux try xclip, then wl-copy. Errors are non-fatal.
    char cmd[1024];
    int ret = snprintf(cmd, sizeof(cmd),
        "(xclip -selection clipboard -t image/png -i \"%s\""
        " || wl-copy --type image/png < \"%s\") >/dev/null 2>&1 &",
        filename, filename);
    if (ret <= 0 || (size_t) ret >= sizeof(cmd)) {
        LOGW("Could not build clipboard command");
        return;
    }
    int rc = system(cmd);
    if (rc != 0) {
        LOGW("Could not copy screenshot to clipboard (rc=%d)", rc);
    }
#endif
}

static bool
generate_filename(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm) {
        return false;
    }
    int ret = snprintf(buf, size,
                       "%s/Downloads/scrcpy_%04d%02d%02d-%02d%02d%02d.png",
                       home,
                       tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                       tm->tm_hour, tm->tm_min, tm->tm_sec);
    return ret > 0 && (size_t) ret < size;
}

static AVFrame *
convert_to_rgb(const AVFrame *frame) {
    AVFrame *rgb_frame = av_frame_alloc();
    if (!rgb_frame) {
        LOG_OOM();
        return NULL;
    }

    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = frame->width;
    rgb_frame->height = frame->height;

    if (av_image_alloc(rgb_frame->data, rgb_frame->linesize,
                       rgb_frame->width, rgb_frame->height,
                       AV_PIX_FMT_RGB24, 1) < 0) {
        LOG_OOM();
        av_frame_free(&rgb_frame);
        return NULL;
    }

    struct SwsContext *sws = sws_getContext(
        frame->width, frame->height, frame->format,
        rgb_frame->width, rgb_frame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws) {
        LOGE("Could not create sws context for screenshot");
        av_freep(&rgb_frame->data[0]);
        av_frame_free(&rgb_frame);
        return NULL;
    }

    sws_scale(sws, (const uint8_t *const *) frame->data, frame->linesize,
              0, frame->height, rgb_frame->data, rgb_frame->linesize);

    sws_freeContext(sws);
    return rgb_frame;
}

static AVFrame *
apply_orientation(const AVFrame *frame, enum sc_orientation orientation) {
    if (orientation == SC_ORIENTATION_0) {
        return NULL; // no transformation needed
    }

    bool swap = sc_orientation_is_swap(orientation);
    bool hmirror = sc_orientation_is_mirror(orientation);
    enum sc_orientation rotation = sc_orientation_get_rotation(orientation);

    int src_w = frame->width;
    int src_h = frame->height;
    int dst_w = swap ? src_h : src_w;
    int dst_h = swap ? src_w : src_h;

    AVFrame *out = av_frame_alloc();
    if (!out) {
        LOG_OOM();
        return NULL;
    }

    out->format = frame->format;
    out->width = dst_w;
    out->height = dst_h;

    if (av_image_alloc(out->data, out->linesize, dst_w, dst_h,
                       out->format, 1) < 0) {
        LOG_OOM();
        av_frame_free(&out);
        return NULL;
    }

    const uint8_t *src = frame->data[0];
    uint8_t *dst = out->data[0];
    int src_stride = frame->linesize[0];
    int dst_stride = out->linesize[0];
    int bpp = 3; // RGB24

    for (int y = 0; y < src_h; y++) {
        for (int x = 0; x < src_w; x++) {
            int sx = x;
            int sy = y;

            // Apply mirror first
            if (hmirror) {
                sx = src_w - 1 - sx;
            }

            // Apply rotation
            int dx, dy;
            switch (rotation) {
                case SC_ORIENTATION_0:
                    dx = sx;
                    dy = sy;
                    break;
                case SC_ORIENTATION_90:
                    dx = src_h - 1 - sy;
                    dy = sx;
                    break;
                case SC_ORIENTATION_180:
                    dx = src_w - 1 - sx;
                    dy = src_h - 1 - sy;
                    break;
                case SC_ORIENTATION_270:
                    dx = sy;
                    dy = src_w - 1 - sx;
                    break;
                default:
                    dx = sx;
                    dy = sy;
                    break;
            }

            memcpy(dst + dy * dst_stride + dx * bpp,
                   src + y * src_stride + x * bpp,
                   bpp);
        }
    }

    return out;
}

static bool
encode_png(const AVFrame *frame, const char *filename) {
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!codec) {
        LOGE("PNG encoder not found");
        return false;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        LOG_OOM();
        return false;
    }

    ctx->width = frame->width;
    ctx->height = frame->height;
    ctx->pix_fmt = AV_PIX_FMT_RGB24;
    ctx->time_base = (AVRational){1, 1};

    if (avcodec_open2(ctx, codec, NULL) < 0) {
        LOGE("Could not open PNG encoder");
        avcodec_free_context(&ctx);
        return false;
    }

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        LOG_OOM();
        avcodec_free_context(&ctx);
        return false;
    }

    bool ok = false;

    int ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        LOGE("Could not send frame to PNG encoder");
        goto end;
    }

    ret = avcodec_receive_packet(ctx, pkt);
    if (ret < 0) {
        LOGE("Could not receive packet from PNG encoder");
        goto end;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        LOGE("Could not open file: %s", filename);
        goto end;
    }

    if (fwrite(pkt->data, 1, pkt->size, f) != (size_t) pkt->size) {
        LOGE("Could not write screenshot to %s", filename);
        fclose(f);
        goto end;
    }

    fclose(f);
    ok = true;

end:
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    return ok;
}

bool
sc_screenshot_save(const AVFrame *frame, enum sc_orientation orientation) {
    LOGD("Screenshot save requested (orientation=%d)", orientation);

    if (!frame || !frame->data[0]) {
        LOGW("No frame available for screenshot");
        return false;
    }

    LOGD("Frame available: %dx%d format=%d",
         frame->width, frame->height, frame->format);

    char filename[512];
    if (!generate_filename(filename, sizeof(filename))) {
        LOGE("Could not generate screenshot filename");
        return false;
    }

    LOGD("Screenshot target path: %s", filename);

    AVFrame *rgb_frame = convert_to_rgb(frame);
    if (!rgb_frame) {
        return false;
    }

    LOGD("Converted to RGB: %dx%d", rgb_frame->width, rgb_frame->height);

    // Apply orientation to the RGB frame
    AVFrame *oriented = apply_orientation(rgb_frame, orientation);
    const AVFrame *final_frame = oriented ? oriented : rgb_frame;

    if (oriented) {
        LOGD("Orientation applied: %dx%d", oriented->width, oriented->height);
    } else {
        LOGD("No orientation transform needed");
    }

    LOGD("Encoding PNG...");
    bool ok = encode_png(final_frame, filename);
    if (ok) {
        LOGI("Screenshot saved to %s", filename);
        copy_png_to_clipboard(filename);
    } else {
        LOGE("Failed to encode/save screenshot");
    }

    if (oriented) {
        av_freep(&oriented->data[0]);
        av_frame_free(&oriented);
    }
    av_freep(&rgb_frame->data[0]);
    av_frame_free(&rgb_frame);

    return ok;
}
