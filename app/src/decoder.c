#include "decoder.h"

#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
#include <unistd.h>

#include "config.h"
#include "buffer_util.h"
#include "events.h"
#include "frames.h"
#include "lock_util.h"
#include "log.h"
#include "recorder.h"

#define BUFSIZE 0x10000

#define HEADER_SIZE 12
#define NO_PTS UINT64_C(-1)

static struct frame_meta *frame_meta_new(uint64_t pts) {
    struct frame_meta *meta = malloc(sizeof(*meta));
    if (!meta) {
        return meta;
    }
    meta->pts = pts;
    meta->next = NULL;
    return meta;
}

static void frame_meta_delete(struct frame_meta *frame_meta) {
    free(frame_meta);
}

static SDL_bool receiver_state_push_meta(struct receiver_state *state,
                                         uint64_t pts) {
    struct frame_meta *frame_meta = frame_meta_new(pts);
    if (!frame_meta) {
        return SDL_FALSE;
    }

    // append to the list
    // (iterate to find the last item, in practice the list should be tiny)
    struct frame_meta **p = &state->frame_meta_queue;
    while (*p) {
        p = &(*p)->next;
    }
    *p = frame_meta;
    return SDL_TRUE;
}

static uint64_t receiver_state_take_meta(struct receiver_state *state) {
    struct frame_meta *frame_meta = state->frame_meta_queue; // first item
    SDL_assert(frame_meta); // must not be empty
    uint64_t pts = frame_meta->pts;
    state->frame_meta_queue = frame_meta->next; // remove the item
    frame_meta_delete(frame_meta);
    return pts;
}

static int read_packet_with_meta(void *opaque, uint8_t *buf, int buf_size) {
    struct decoder *decoder = opaque;
    struct receiver_state *state = &decoder->receiver_state;

    // The video stream contains raw packets, without time information. When we
    // record, we retrieve the timestamps separately, from a "meta" header
    // added by the server before each raw packet.
    //
    // The "meta" header length is 12 bytes:
    // [. . . . . . . .|. . . .]. . . . . . . . . . . . . . . ...
    //  <-------------> <-----> <-----------------------------...
    //        PTS        packet        raw packet
    //                    size
    //
    // It is followed by <packet_size> bytes containing the packet/frame.

    if (!state->remaining) {
#define HEADER_SIZE 12
        uint8_t header[HEADER_SIZE];
        ssize_t r = net_recv_all(decoder->video_socket, header, HEADER_SIZE);
        if (r == -1) {
            return AVERROR(errno);
        }
        if (r == 0) {
            return AVERROR_EOF;
        }
        // no partial read (net_recv_all())
        SDL_assert_release(r == HEADER_SIZE);

        uint64_t pts = buffer_read64be(header);
        state->remaining = buffer_read32be(&header[8]);

        if (pts != NO_PTS && !receiver_state_push_meta(state, pts)) {
            LOGE("Could not store PTS for recording");
            // we cannot save the PTS, the recording would be broken
            return AVERROR(ENOMEM);
        }
    }

    SDL_assert(state->remaining);

    if (buf_size > state->remaining)
        buf_size = state->remaining;

    ssize_t r = net_recv(decoder->video_socket, buf, buf_size);
    if (r == -1) {
        return AVERROR(errno);
    }
    if (r == 0) {
        return AVERROR_EOF;
    }

    SDL_assert(state->remaining >= r);
    state->remaining -= r;

    return r;
}

static int read_raw_packet(void *opaque, uint8_t *buf, int buf_size) {
    struct decoder *decoder = opaque;
    ssize_t r = net_recv(decoder->video_socket, buf, buf_size);
    if (r == -1) {
        return AVERROR(errno);
    }
    if (r == 0) {
        return AVERROR_EOF;
    }
    return r;
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

    // initialize the receiver state
    decoder->receiver_state.frame_meta_queue = NULL;
    decoder->receiver_state.remaining = 0;

    // if recording is enabled, a "header" is sent between raw packets
    int (*read_packet)(void *, uint8_t *, int) =
            decoder->recorder ? read_packet_with_meta : read_raw_packet;
    AVIOContext *avio_ctx = avio_alloc_context(buffer, BUFSIZE, 0, decoder,
                                               read_packet, NULL, NULL);
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

    if (decoder->recorder &&
            !recorder_open(decoder->recorder, codec)) {
        LOGE("Could not open recorder");
        goto run_finally_close_input;
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
                av_packet_unref(&packet);
                goto run_quit;
            }
            if (got_picture) {
                push_frame(decoder);
            }
            packet.size -= len;
            packet.data += len;
        }
#endif

        if (decoder->recorder) {
            // we retrieve the PTS in order they were received, so they will
            // be assigned to the correct frame
            uint64_t pts = receiver_state_take_meta(&decoder->receiver_state);
            packet.pts = pts;
            packet.dts = pts;

            // no need to rescale with av_packet_rescale_ts(), the timestamps
            // are in microseconds both in input and output
            if (!recorder_write(decoder->recorder, &packet)) {
                LOGE("Could not write frame to output file");
                av_packet_unref(&packet);
                goto run_quit;
            }
        }

        av_packet_unref(&packet);

        if (avio_ctx->eof_reached) {
            break;
        }
    }

    LOGD("End of frames");

run_quit:
    if (decoder->recorder) {
        recorder_close(decoder->recorder);
    }
run_finally_close_input:
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

void decoder_init(struct decoder *decoder, struct frames *frames,
                  socket_t video_socket, struct recorder *recorder) {
    decoder->frames = frames;
    decoder->video_socket = video_socket;
    decoder->recorder = recorder;
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
