/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * video decoding with libavcodec API example
 *
 * @example decode_video.c
 */

#include "capture.h"

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL_events.h>

#include "events.h"
#include "util/lock.h"
#include "util/log.h"

static const char *string_error(int err) {
  switch (err) {
    case AVERROR_EOF: return "End of File";
    case AVERROR_BUG: return "Bug";
    case AVERROR_BUG2: return "Bug2";
    case AVERROR_BUFFER_TOO_SMALL: return "Buffer Too Small";
    case AVERROR_EXIT: return "Exit Requested";
    case AVERROR_DECODER_NOT_FOUND: return "Decoder Not Found";
    case AVERROR_DEMUXER_NOT_FOUND: return "Demuxer Not Found";
    case AVERROR_ENCODER_NOT_FOUND: return "Encoder Not Found";
    case AVERROR_EXTERNAL: return "External Error";
    case AVERROR_FILTER_NOT_FOUND: return "Filter Not Found";
    case AVERROR_MUXER_NOT_FOUND: return "Muxer Not Found";
    case AVERROR_OPTION_NOT_FOUND: return "Option Not Found";
    case AVERROR_PATCHWELCOME: return "Patch Welcome";
    case AVERROR_PROTOCOL_NOT_FOUND: return "Protocol Not Found";
    case AVERROR_STREAM_NOT_FOUND: return "Stream Not Found";
    case AVERROR_INVALIDDATA: return "Invalid Data (waiting for index frame?)";
    case AVERROR(EINVAL): return "Invalid";
    case AVERROR(ENOMEM): return "No Memory";
    case AVERROR(EAGAIN): return "Try Again";
    default: return "Unknown";
  }
}

static void notify_complete() {
  static SDL_Event new_frame_event = {
    .type = EVENT_SCREEN_CAPTURE_COMPLETE,
  };
  SDL_PushEvent(&new_frame_event);
}

// Thanks, https://stackoverflow.com/a/18313791
// Also, https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/doc/examples/decode_video.c

static bool in_frame_to_png(
  AVIOContext *output_context,
  AVCodecContext *codecCtx, AVFrame *inframe) {
  uint64_t start = get_timestamp();
  int targetHeight = codecCtx->height;
  int targetWidth = codecCtx->width;
  struct SwsContext * swCtx = sws_getContext(codecCtx->width,
    codecCtx->height,
    codecCtx->pix_fmt,
    targetWidth,
    targetHeight,
    AV_PIX_FMT_RGB24,
    // NOTE(frankleonrose): These options offer better picture
    // quality in case of scaling, according to https://stackoverflow.com/a/46169884
    // SWS_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND,
    SWS_FAST_BILINEAR,
    0, 0, 0);

  AVFrame * rgbFrame = av_frame_alloc();
  LOGV("Image frame width: %d height: %d scaling to %d x %d", *inframe->linesize, inframe->height, targetWidth, targetHeight);
  rgbFrame->width = targetWidth;
  rgbFrame->height = targetHeight;
  rgbFrame->format = AV_PIX_FMT_RGB24;
  av_image_alloc(
    rgbFrame->data, rgbFrame->linesize, targetWidth, targetHeight, AV_PIX_FMT_RGB24, 1);
  sws_scale(
    swCtx,
    // const_cast<const uint8_t * const*>(inframe->data)
    (const uint8_t * const*)(inframe->data),
    inframe->linesize, 0,
    inframe->height, rgbFrame->data, rgbFrame->linesize);

  LOGV("Scaling image: %llu", get_timestamp() - start);

  av_dict_set(&rgbFrame->metadata,
              "Software", "scrcpy/" SCRCPY_VERSION, 0);

  AVCodec *outCodec = avcodec_find_encoder(AV_CODEC_ID_PNG);
  if (!outCodec) {
    LOGE("Failed to find PNG codec");
    return false;
  }
  AVCodecContext *outCodecCtx = avcodec_alloc_context3(outCodec);
  if (!outCodecCtx) {
    LOGE("Unable to allocate PNG codec context");
    return false;
  }

  outCodecCtx->width = targetWidth;
  outCodecCtx->height = targetHeight;
  outCodecCtx->pix_fmt = AV_PIX_FMT_RGB24;
  outCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
  if (codecCtx->time_base.num && codecCtx->time_base.num) {
    outCodecCtx->time_base.num = codecCtx->time_base.num;
    outCodecCtx->time_base.den = codecCtx->time_base.den;
  } else {
    outCodecCtx->time_base.num = 1;
    outCodecCtx->time_base.den = 10;  // FPS
  }

  int ret = avcodec_open2(outCodecCtx, outCodec, NULL);
  if (ret < 0) {
    LOGE("Failed to open PNG codec: %s", string_error(ret));
    return false;
  }

  ret = avcodec_send_frame(outCodecCtx, rgbFrame);
  av_frame_free(&rgbFrame);
  if (ret < 0) {
    LOGE("Failed to send frame to output codex");
    return false;
  }

  AVPacket outPacket;
  av_init_packet(&outPacket);
  outPacket.size = 0;
  outPacket.data = NULL;
  ret = avcodec_receive_packet(outCodecCtx, &outPacket);
  avcodec_close(outCodecCtx);
  av_free(outCodecCtx);
  LOGV("Converted to PNG: %llu", get_timestamp() - start);
  if (ret >= 0) {
    // Dump packet
    avio_write(output_context, outPacket.data, outPacket.size);
    notify_complete();
    av_packet_unref(&outPacket);
    LOGV("Wrote file: %llu", get_timestamp() - start);
    log_timestamp("Capture written");
    return true;
  } else {
    LOGE("Failed to receive packet");
    return false;
  }
}

// Decodes a video packet into PNG image, if possible.
// Sends a new video packet `packet` into the active decoding
// context `dec_ctx`.
// Returns `true` when the decoding yielded a PNG.
static bool decode_packet_to_png(
    AVIOContext *output_context,
    AVCodecContext *dec_ctx,
    const AVPacket *packet,
    AVFrame *working_frame) {
  LOGV("Sending video packet: %p Size: %d", packet, packet ? packet->size : 0);

  int ret = avcodec_send_packet(dec_ctx, packet);
  if (ret < 0) {
    LOGW( "Error sending a packet for decoding: %s", string_error(ret));
    return false;
  }

  bool found = false;
  while (ret >= 0) {
    ret = avcodec_receive_frame(dec_ctx, working_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      LOGV("No frame received: %s", string_error(ret));
      break;  // We're done
    } else if (ret < 0) {
      LOGW("Error during decoding: %s", string_error(ret));
    }

    LOGI("Found frame: %d", dec_ctx->frame_number);
    if (in_frame_to_png(output_context, dec_ctx, working_frame)) {
      found = true;
      break;
    } else {
      LOGE("Failed to generate PNG");
    }
  }
  return found;
}

bool capture_init(struct capture *capture, const char *filename) {
  capture->filename = SDL_strdup(filename);
  if (!capture->filename) {
    LOGE("Could not strdup filename");
    return false;
  }

  capture->mutex = SDL_CreateMutex();
  if (!capture->mutex) {
    LOGC("Could not create mutex");
    SDL_free(capture->filename);
    return false;
  }

  capture->queue_cond = SDL_CreateCond();
  if (!capture->queue_cond) {
    LOGC("Could not create capture cond");
    SDL_DestroyMutex(capture->mutex);
    SDL_free(capture->filename);
    return false;
  }

  queue_init(&capture->queue);
  capture->stopped = false;
  capture->finished = false;

  int ret = avio_open(&capture->file_context, capture->filename,
                        AVIO_FLAG_WRITE);
  if (ret < 0) {
    LOGE("Failed to open output file: %s", capture->filename);
    return false;
  }

  /* find the MPEG-1 video decoder */
  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) {
    LOGE("Codec not found: %d", AV_CODEC_ID_H264);
    return false;
  }

  capture->context = avcodec_alloc_context3(codec);
  if (!capture->context) {
    LOGE("Could not allocate video codec context");
    return false;
  }

  /* For some codecs, such as msmpeg4 and mpeg4, width and height
     MUST be initialized there because this information is not
     available in the bitstream. */

  /* open it */
  if (avcodec_open2(capture->context, codec, NULL) < 0) {
    LOGE("Could not open codec");
    return false;
  }

  // Create a reusable frame buffer.
  capture->working_frame = av_frame_alloc();
  if (!capture->working_frame) {
    LOGE("Could not allocate video frame");
    return false;
  }
  return true;
}

void capture_destroy(struct capture *capture) {
  av_frame_free(&capture->working_frame);
  avcodec_free_context(&capture->context);
  avio_close(capture->file_context);
}

static bool capture_process(struct capture *capture, const AVPacket *packet) {
  log_timestamp("Processing packet");
  static uint64_t total = 0;
  uint64_t start = get_timestamp();
  if (capture->finished) {
    LOGV("Skipping redundant call to capture_push");
  } else {
    bool found_png = decode_packet_to_png(capture->file_context, capture->context, packet, capture->working_frame);
    if (found_png) {
      LOGI("Parsed PNG from incoming packets.");
      capture->finished = found_png;
    }
  }
  uint64_t duration = get_timestamp() - start;
  total += duration;
  LOGV("Capture step microseconds: %llu total:  %llu", duration, total);
  return capture->finished;
}

static struct capture_packet *
capture_packet_new(const AVPacket *packet) {
    struct capture_packet *rec = SDL_malloc(sizeof(*rec));
    if (!rec) {
        return NULL;
    }

    // av_packet_ref() does not initialize all fields in old FFmpeg versions
    // See <https://github.com/Genymobile/scrcpy/issues/707>
    av_init_packet(&rec->packet);

    if (av_packet_ref(&rec->packet, packet)) {
        SDL_free(rec);
        return NULL;
    }
    return rec;
}

static void
capture_packet_delete(struct capture_packet *rec) {
    av_packet_unref(&rec->packet);
    SDL_free(rec);
}

static int
run_capture(void *data) {
    log_timestamp("Running capture thread");
    struct capture *capture = data;

    for (;;) {
        mutex_lock(capture->mutex);

        while (!capture->stopped && queue_is_empty(&capture->queue)) {
            cond_wait(capture->queue_cond, capture->mutex);
        }

        // if stopped is set, continue to process the remaining events (to
        // finish the capture) before actually stopping

        if (capture->stopped && queue_is_empty(&capture->queue)) {
            mutex_unlock(capture->mutex);
            break;
        }

        struct capture_packet *rec;
        queue_take(&capture->queue, next, &rec);

        mutex_unlock(capture->mutex);

        bool ok = capture_process(capture, &rec->packet);
        capture_packet_delete(rec);
        if (ok) {
            break;
        }
    }

    LOGD("capture thread ended");

    return 0;
}

bool
capture_start(struct capture *capture) {
    log_timestamp("Starting capture thread");

    capture->thread = SDL_CreateThread(run_capture, "capture", capture);
    if (!capture->thread) {
        LOGC("Could not start capture thread");
        return false;
    }

    return true;
}

void
capture_stop(struct capture *capture) {
    mutex_lock(capture->mutex);
    capture->stopped = true;
    cond_signal(capture->queue_cond);
    mutex_unlock(capture->mutex);
}

void
capture_join(struct capture *capture) {
    SDL_WaitThread(capture->thread, NULL);
}

bool
capture_push(struct capture *capture, const AVPacket *packet) {
    log_timestamp("Received packet");
    mutex_lock(capture->mutex);
    assert(!capture->stopped);

    if (capture->finished) {
        // reject any new packet (this will stop the stream)
        return false;
    }

    struct capture_packet *rec = capture_packet_new(packet);
    if (!rec) {
        LOGC("Could not allocate capture packet");
        return false;
    }

    queue_push(&capture->queue, next, rec);
    cond_signal(capture->queue_cond);

    mutex_unlock(capture->mutex);
    return true;
}
