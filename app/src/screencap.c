#include "screencap.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "util/log.h"

/** Downcast packet sink to screencap */
#define DOWNCAST_VIDEO(SINK) \
    container_of(SINK, struct sc_screencap, video_packet_sink)

static bool
sc_screencap_save_frame_as_png(const char *filename, AVFrame *frame) {
    bool success = false;

    const AVCodec *png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!png_codec) {
        LOGE("PNG encoder not found");
        return false;
    }

    AVCodecContext *png_ctx = avcodec_alloc_context3(png_codec);
    if (!png_ctx) {
        LOG_OOM();
        return false;
    }

    png_ctx->width = frame->width;
    png_ctx->height = frame->height;
    png_ctx->pix_fmt = AV_PIX_FMT_RGB24;
    png_ctx->time_base = (AVRational){1, 1};

    int ret = avcodec_open2(png_ctx, png_codec, NULL);
    if (ret < 0) {
        LOGE("Could not open PNG encoder");
        goto free_png_ctx;
    }

    // Convert frame to RGB24 if needed
    AVFrame *rgb_frame = av_frame_alloc();
    if (!rgb_frame) {
        LOG_OOM();
        goto free_png_ctx;
    }

    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = frame->width;
    rgb_frame->height = frame->height;

    ret = av_frame_get_buffer(rgb_frame, 0);
    if (ret < 0) {
        LOGE("Could not allocate RGB frame buffer");
        goto free_rgb_frame;
    }

    struct SwsContext *sws_ctx = sws_getContext(
        frame->width, frame->height, frame->format,
        rgb_frame->width, rgb_frame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        LOGE("Could not create sws context");
        goto free_rgb_frame;
    }

    sws_scale(sws_ctx, (const uint8_t * const *)frame->data, frame->linesize,
              0, frame->height, rgb_frame->data, rgb_frame->linesize);
    sws_freeContext(sws_ctx);

    // Encode to PNG
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        LOG_OOM();
        goto free_rgb_frame;
    }

    ret = avcodec_send_frame(png_ctx, rgb_frame);
    if (ret < 0) {
        LOGE("Could not send frame to PNG encoder");
        goto free_pkt;
    }

    ret = avcodec_receive_packet(png_ctx, pkt);
    if (ret < 0) {
        LOGE("Could not receive PNG packet");
        goto free_pkt;
    }

    // Write PNG data to file
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        LOGE("Could not open output file: %s", filename);
        goto free_pkt;
    }

    size_t written = fwrite(pkt->data, 1, pkt->size, fp);
    fclose(fp);

    if (written != (size_t)pkt->size) {
        LOGE("Failed to write PNG data to %s", filename);
        goto free_pkt;
    }

    success = true;

free_pkt:
    av_packet_free(&pkt);
free_rgb_frame:
    av_frame_free(&rgb_frame);
free_png_ctx:
    avcodec_free_context(&png_ctx);

    return success;
}

static bool
sc_screencap_decode_and_save(struct sc_screencap *screencap) {
    AVCodecContext *ctx = screencap->codec_ctx;

    // Set extradata from config packet if available
    if (screencap->has_config && screencap->config_packet) {
        uint8_t *extradata = av_malloc(screencap->config_packet->size
                                       + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!extradata) {
            LOG_OOM();
            return false;
        }
        memcpy(extradata, screencap->config_packet->data,
               screencap->config_packet->size);
        memset(extradata + screencap->config_packet->size, 0,
               AV_INPUT_BUFFER_PADDING_SIZE);
        av_free(ctx->extradata);
        ctx->extradata = extradata;
        ctx->extradata_size = screencap->config_packet->size;
    }

    int ret = avcodec_send_packet(ctx, screencap->video_packet);
    if (ret < 0) {
        LOGE("Screencap: could not send video packet to decoder: %d", ret);
        return false;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        LOG_OOM();
        return false;
    }

    ret = avcodec_receive_frame(ctx, frame);
    if (ret < 0) {
        LOGE("Screencap: could not receive decoded frame: %d", ret);
        av_frame_free(&frame);
        return false;
    }

    bool ok = sc_screencap_save_frame_as_png(screencap->filename, frame);
    av_frame_free(&frame);

    if (ok) {
        LOGI("Screenshot saved to %s", screencap->filename);
    }

    return ok;
}

static int
run_screencap(void *data) {
    struct sc_screencap *screencap = data;

    sc_mutex_lock(&screencap->mutex);

    while (!screencap->stopped && !screencap->video_packet_ready) {
        sc_cond_wait(&screencap->cond, &screencap->mutex);
    }

    if (screencap->stopped && !screencap->video_packet_ready) {
        sc_mutex_unlock(&screencap->mutex);
        screencap->cbs->on_ended(screencap, false, screencap->cbs_userdata);
        return 0;
    }

    sc_mutex_unlock(&screencap->mutex);

    bool success = sc_screencap_decode_and_save(screencap);

    // Signal that we are done - stop accepting packets
    sc_mutex_lock(&screencap->mutex);
    screencap->stopped = true;
    screencap->captured = true;
    sc_mutex_unlock(&screencap->mutex);

    screencap->cbs->on_ended(screencap, success, screencap->cbs_userdata);

    return 0;
}

static bool
sc_screencap_video_packet_sink_open(struct sc_packet_sink *sink,
                                    AVCodecContext *ctx) {
    struct sc_screencap *screencap = DOWNCAST_VIDEO(sink);

    // Create our own decoder context using the same codec
    const AVCodec *codec = ctx->codec;
    AVCodecContext *dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        LOG_OOM();
        return false;
    }

    // Copy parameters from the source context
    dec_ctx->width = ctx->width;
    dec_ctx->height = ctx->height;
    dec_ctx->pix_fmt = ctx->pix_fmt;
    dec_ctx->time_base = ctx->time_base;

    if (ctx->extradata_size > 0) {
        dec_ctx->extradata = av_malloc(ctx->extradata_size
                                       + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!dec_ctx->extradata) {
            LOG_OOM();
            avcodec_free_context(&dec_ctx);
            return false;
        }
        memcpy(dec_ctx->extradata, ctx->extradata, ctx->extradata_size);
        memset(dec_ctx->extradata + ctx->extradata_size, 0,
               AV_INPUT_BUFFER_PADDING_SIZE);
        dec_ctx->extradata_size = ctx->extradata_size;
    }

    int ret = avcodec_open2(dec_ctx, codec, NULL);
    if (ret < 0) {
        LOGE("Screencap: could not open codec: %d", ret);
        avcodec_free_context(&dec_ctx);
        return false;
    }

    screencap->codec_ctx = dec_ctx;

    return true;
}

static void
sc_screencap_video_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_screencap *screencap = DOWNCAST_VIDEO(sink);

    sc_mutex_lock(&screencap->mutex);
    screencap->stopped = true;
    sc_cond_signal(&screencap->cond);
    sc_mutex_unlock(&screencap->mutex);
}

static bool
sc_screencap_video_packet_sink_push(struct sc_packet_sink *sink,
                                    const AVPacket *packet) {
    struct sc_screencap *screencap = DOWNCAST_VIDEO(sink);

    sc_mutex_lock(&screencap->mutex);

    if (screencap->stopped || screencap->video_packet_ready) {
        // Already captured or stopped, reject further packets
        sc_mutex_unlock(&screencap->mutex);
        return false;
    }

    bool is_config = packet->pts == AV_NOPTS_VALUE;
    if (is_config) {
        // Store the config packet (contains codec extradata like SPS/PPS)
        if (screencap->config_packet) {
            av_packet_free(&screencap->config_packet);
        }
        screencap->config_packet = av_packet_alloc();
        if (!screencap->config_packet) {
            LOG_OOM();
            sc_mutex_unlock(&screencap->mutex);
            return false;
        }
        if (av_packet_ref(screencap->config_packet, packet) < 0) {
            av_packet_free(&screencap->config_packet);
            sc_mutex_unlock(&screencap->mutex);
            return false;
        }
        screencap->has_config = true;
        sc_mutex_unlock(&screencap->mutex);
        return true;
    }

    // This is a real video packet (keyframe) - capture it
    screencap->video_packet = av_packet_alloc();
    if (!screencap->video_packet) {
        LOG_OOM();
        sc_mutex_unlock(&screencap->mutex);
        return false;
    }
    if (av_packet_ref(screencap->video_packet, packet) < 0) {
        av_packet_free(&screencap->video_packet);
        sc_mutex_unlock(&screencap->mutex);
        return false;
    }

    screencap->video_packet_ready = true;
    sc_cond_signal(&screencap->cond);
    sc_mutex_unlock(&screencap->mutex);

    // Return false to detach from the demuxer pipeline after first frame
    // This is intentional - we only need one frame
    return false;
}

bool
sc_screencap_init(struct sc_screencap *screencap, const char *filename,
                  const struct sc_screencap_callbacks *cbs,
                  void *cbs_userdata) {
    screencap->filename = strdup(filename);
    if (!screencap->filename) {
        LOG_OOM();
        return false;
    }

    bool ok = sc_mutex_init(&screencap->mutex);
    if (!ok) {
        goto error_free_filename;
    }

    ok = sc_cond_init(&screencap->cond);
    if (!ok) {
        goto error_mutex_destroy;
    }

    screencap->codec_ctx = NULL;
    screencap->config_packet = NULL;
    screencap->has_config = false;
    screencap->captured = false;
    screencap->stopped = false;
    screencap->video_packet = NULL;
    screencap->video_packet_ready = false;

    assert(cbs && cbs->on_ended);
    screencap->cbs = cbs;
    screencap->cbs_userdata = cbs_userdata;

    static const struct sc_packet_sink_ops video_ops = {
        .open = sc_screencap_video_packet_sink_open,
        .close = sc_screencap_video_packet_sink_close,
        .push = sc_screencap_video_packet_sink_push,
    };

    screencap->video_packet_sink.ops = &video_ops;

    return true;

error_mutex_destroy:
    sc_mutex_destroy(&screencap->mutex);
error_free_filename:
    free(screencap->filename);

    return false;
}

bool
sc_screencap_start(struct sc_screencap *screencap) {
    bool ok = sc_thread_create(&screencap->thread, run_screencap,
                               "scrcpy-screencap", screencap);
    if (!ok) {
        LOGE("Could not start screencap thread");
        return false;
    }

    return true;
}

void
sc_screencap_stop(struct sc_screencap *screencap) {
    sc_mutex_lock(&screencap->mutex);
    screencap->stopped = true;
    sc_cond_signal(&screencap->cond);
    sc_mutex_unlock(&screencap->mutex);
}

void
sc_screencap_join(struct sc_screencap *screencap) {
    sc_thread_join(&screencap->thread, NULL);
}

void
sc_screencap_destroy(struct sc_screencap *screencap) {
    if (screencap->codec_ctx) {
        avcodec_free_context(&screencap->codec_ctx);
    }
    if (screencap->config_packet) {
        av_packet_free(&screencap->config_packet);
    }
    if (screencap->video_packet) {
        av_packet_free(&screencap->video_packet);
    }
    sc_cond_destroy(&screencap->cond);
    sc_mutex_destroy(&screencap->mutex);
    free(screencap->filename);
}
