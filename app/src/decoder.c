#include "decoder.h"

#include <libavformat/avformat.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_net.h>
#include <SDL2/SDL_thread.h>
#include <unistd.h>

#include "events.h"
#include "frames.h"
#include "lockutil.h"
#include "netutil.h"

#define BUFSIZE 0x10000

static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    struct decoder *decoder = opaque;
    return SDLNet_TCP_Recv(decoder->video_socket, buf, buf_size);
}

// set the decoded frame as ready for rendering, and notify
static void push_frame(struct decoder *decoder) {
    struct frames *frames = decoder->frames;
    mutex_lock(frames->mutex);
    if (!decoder->skip_frames) {
        while (!frames->rendering_frame_consumed) {
            SDL_CondWait(frames->rendering_frame_consumed_cond, frames->mutex);
        }
    } else if (!frames->rendering_frame_consumed) {
        SDL_LogInfo(SDL_LOG_CATEGORY_RENDER, "Skip frame");
    }

    frames_swap(frames);
    frames->rendering_frame_consumed = SDL_FALSE;
    mutex_unlock(frames->mutex);

    static SDL_Event new_frame_event = {
        .type = EVENT_NEW_FRAME,
    };
    SDL_PushEvent(&new_frame_event);
}

static int run_decoder(void *data) {
    struct decoder *decoder = data;
    int ret = 0;

    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "H.264 decoder not found");
        return -1;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Could not allocate decoder context");
        return -1;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not open H.264 codec");
        ret = -1;
        goto run_finally_free_codec_ctx;
    }

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Could not allocate format context");
        ret = -1;
        goto run_finally_close_codec;
    }

    unsigned char *buffer = av_malloc(BUFSIZE);
    if (!buffer) {
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Could not allocate buffer");
        ret = -1;
        goto run_finally_free_format_ctx;
    }

    AVIOContext *avio_ctx = avio_alloc_context(buffer, BUFSIZE, 0, decoder, read_packet, NULL, NULL);
    if (!avio_ctx) {
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Could not allocate avio context");
        // avformat_open_input takes ownership of 'buffer'
        // so only free the buffer before avformat_open_input()
        av_free(buffer);
        ret = -1;
        goto run_finally_free_format_ctx;
    }

    format_ctx->pb = avio_ctx;

    //const char *url = "tcp://127.0.0.1:1234";
    if (avformat_open_input(&format_ctx, NULL, NULL, NULL) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not open video stream");
        ret = -1;
        goto run_finally_free_avio_ctx;
    }

    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    while (!av_read_frame(format_ctx, &packet)) {
// the new decoding/encoding API has been introduced by:
// <http://git.videolan.org/?p=ffmpeg.git;a=commitdiff;h=7fc329e2dd6226dfecaa4a1d7adf353bf2773726>
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 37, 0)
        while (packet.size > 0) {
            int got_picture;
            int len = avcodec_decode_video2(codec_ctx, decoder->frames->decoding_frame, &got_picture, &packet);
            if (len < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not decode video packet: %d", len);
                goto run_quit;
            }
            if (got_picture) {
                push_frame(decoder);
            }
            packet.size -= len;
            packet.data += len;
        }
#else
        int ret;
        if ((ret = avcodec_send_packet(codec_ctx, &packet)) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not send video packet: %d", ret);
            goto run_quit;
        }
        if ((ret = avcodec_receive_frame(codec_ctx, decoder->frames->decoding_frame)) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not receive video frame: %d", ret);
            goto run_quit;
        }

        push_frame(decoder);
#endif
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "End of frames");

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
    SDL_PushEvent(&(SDL_Event) {.type = EVENT_DECODER_STOPPED});
    return ret;
}

SDL_bool decoder_start(struct decoder *decoder) {
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Starting decoder thread");

    decoder->thread = SDL_CreateThread(run_decoder, "video_decoder", decoder);
    if (!decoder->thread) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Could not start decoder thread");
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void decoder_join(struct decoder *decoder) {
    SDL_WaitThread(decoder->thread, NULL);
}
