#include "decoder.h"

#include <libavformat/avformat.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
#include <unistd.h>

#include "config.h"
#include "events.h"
#include "frames.h"
#include "lockutil.h"
#include "log.h"

#define BUFSIZE 0x10000

static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    struct decoder *decoder = opaque;
    return net_recv(decoder->video_socket, buf, buf_size);
}

// set the decoded frame as ready for rendering, and notify
static void push_frame(struct decoder *decoder) {
    SDL_bool previous_frame_consumed = frames_offer_decoded_frame(decoder->frames);
    if (!previous_frame_consumed) {
        // the previous EVENT_NEW_FRAME will consume this frame
        return;
    }
    static SDL_Event new_frame_event = {
        .type = EVENT_NEW_FRAME,
    };
    SDL_PushEvent(&new_frame_event);
}

static void notify_stopped(void) {
    SDL_Event stop_event;
    stop_event.type = EVENT_DECODER_STOPPED;
    SDL_PushEvent(&stop_event);
}

static int run_decoder(void *data) {
    struct decoder *decoder = data;

    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOGE("H.264 decoder not found");
        goto run_end;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        LOGC("Could not allocate decoder context");
        goto run_end;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        LOGE("Could not open H.264 codec");
        goto run_finally_free_codec_ctx;
    }

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        LOGC("Could not allocate format context");
        goto run_finally_close_codec;
    }

    unsigned char *buffer = av_malloc(BUFSIZE);
    if (!buffer) {
        LOGC("Could not allocate buffer");
        goto run_finally_free_format_ctx;
    }

    AVIOContext *avio_ctx = avio_alloc_context(buffer, BUFSIZE, 0, decoder, read_packet, NULL, NULL);
    if (!avio_ctx) {
        LOGC("Could not allocate avio context");
        // avformat_open_input takes ownership of 'buffer'
        // so only free the buffer before avformat_open_input()
        av_free(buffer);
        goto run_finally_free_format_ctx;
    }

    format_ctx->pb = avio_ctx;

    if (avformat_open_input(&format_ctx, NULL, NULL, NULL) < 0) {
        LOGE("Could not open video stream");
        goto run_finally_free_avio_ctx;
    }

    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    while (!av_read_frame(format_ctx, &packet)) {
// the new decoding/encoding API has been introduced by:
// <http://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=7fc329e2dd6226dfecaa4a1d7adf353bf2773726>
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 0)
        int ret;
        if ((ret = avcodec_send_packet(codec_ctx, &packet)) < 0) {
            LOGE("Could not send video packet: %d", ret);
            goto run_quit;
        }
        ret = avcodec_receive_frame(codec_ctx, decoder->frames->decoding_frame);
        if (!ret) {
            // a frame was received
            push_frame(decoder);
        } else if (ret != AVERROR(EAGAIN)) {
            LOGE("Could not receive video frame: %d", ret);
            av_packet_unref(&packet);
            goto run_quit;
        }
#else
        while (packet.size > 0) {
            int got_picture;
            int len = avcodec_decode_video2(codec_ctx, decoder->frames->decoding_frame, &got_picture, &packet);
            if (len < 0) {
                LOGE("Could not decode video packet: %d", len);
                goto run_quit;
            }
            if (got_picture) {
                push_frame(decoder);
            }
            packet.size -= len;
            packet.data += len;
        }
#endif
        av_packet_unref(&packet);

        if (avio_ctx->eof_reached) {
            break;
        }
    }

    LOGD("End of frames");

run_quit:
    avformat_close_input(&format_ctx);
run_finally_free_avio_ctx:
    av_freep(&avio_ctx);
run_finally_free_format_ctx:
    avformat_free_context(format_ctx);
run_finally_close_codec:
    avcodec_close(codec_ctx);
run_finally_free_codec_ctx:
    avcodec_free_context(&codec_ctx);
    notify_stopped();
run_end:
    return 0;
}

void decoder_init(struct decoder *decoder, struct frames *frames, socket_t video_socket) {
    decoder->frames = frames;
    decoder->video_socket = video_socket;
}

SDL_bool decoder_start(struct decoder *decoder) {
    LOGD("Starting decoder thread");

    decoder->thread = SDL_CreateThread(run_decoder, "video_decoder", decoder);
    if (!decoder->thread) {
        LOGC("Could not start decoder thread");
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void decoder_stop(struct decoder *decoder) {
    frames_stop(decoder->frames);
}

void decoder_join(struct decoder *decoder) {
    SDL_WaitThread(decoder->thread, NULL);
}
