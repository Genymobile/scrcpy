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
  struct SwsContext * swCtx = sws_getContext(codecCtx->width,
    codecCtx->height,
    codecCtx->pix_fmt,
    codecCtx->width,
    codecCtx->height,
    AV_PIX_FMT_RGB24,
    SWS_FAST_BILINEAR, 0, 0, 0);

  AVFrame * rgbFrame = av_frame_alloc();
  LOGV("Image frame width: %d height: %d", codecCtx->width, codecCtx->height);
  rgbFrame->width = codecCtx->width;
  rgbFrame->height = codecCtx->height;
  rgbFrame->format = AV_PIX_FMT_RGB24;
  av_image_alloc(
    rgbFrame->data, rgbFrame->linesize, codecCtx->width,
    codecCtx->height, AV_PIX_FMT_RGB24, 1);
  sws_scale(
    swCtx, inframe->data, inframe->linesize, 0,
    inframe->height, rgbFrame->data, rgbFrame->linesize);

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

  outCodecCtx->width = codecCtx->width;
  outCodecCtx->height = codecCtx->height;
  outCodecCtx->pix_fmt = AV_PIX_FMT_RGB24;
  outCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
  if (codecCtx->time_base.num && codecCtx->time_base.num) {
    outCodecCtx->time_base.num = codecCtx->time_base.num;
    outCodecCtx->time_base.den = codecCtx->time_base.den;
  } else {
    outCodecCtx->time_base.num = 1;
    outCodecCtx->time_base.den = 30;  // FPS
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
  if (ret >= 0) {
    // Dump packet
    avio_write(output_context, outPacket.data, outPacket.size);
    notify_complete();
    av_packet_unref(&outPacket);
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

bool capture_push(struct capture *capture, const AVPacket *packet) {
  if (decode_packet_to_png(capture->file_context, capture->context, packet, capture->working_frame)) {
    LOGI("Parsed PNG from incoming packets.");
    return true;
  }
  return false;
}
