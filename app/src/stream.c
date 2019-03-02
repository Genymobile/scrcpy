#include "stream.h"

#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
#include <unistd.h>

#include "compat.h"
#include "config.h"
#include "buffer_util.h"
#include "decoder.h"
#include "events.h"
#include "lock_util.h"
#include "log.h"
#include "recorder.h"

#define BUFSIZE 0x10000

#define HEADER_SIZE 12
#define NO_PTS UINT64_C(-1)

static struct frame_meta *
frame_meta_new(uint64_t pts) {
    struct frame_meta *meta = malloc(sizeof(*meta));
    if (!meta) {
        return meta;
    }
    meta->pts = pts;
    meta->next = NULL;
    return meta;
}

static void
frame_meta_delete(struct frame_meta *frame_meta) {
    free(frame_meta);
}

static bool
receiver_state_push_meta(struct receiver_state *state, uint64_t pts) {
    struct frame_meta *frame_meta = frame_meta_new(pts);
    if (!frame_meta) {
        return false;
    }

    // append to the list
    // (iterate to find the last item, in practice the list should be tiny)
    struct frame_meta **p = &state->frame_meta_queue;
    while (*p) {
        p = &(*p)->next;
    }
    *p = frame_meta;
    return true;
}

static uint64_t
receiver_state_take_meta(struct receiver_state *state) {
    struct frame_meta *frame_meta = state->frame_meta_queue; // first item
    SDL_assert(frame_meta); // must not be empty
    uint64_t pts = frame_meta->pts;
    state->frame_meta_queue = frame_meta->next; // remove the item
    frame_meta_delete(frame_meta);
    return pts;
}

static int
read_packet_with_meta(void *opaque, uint8_t *buf, int buf_size) {
    struct stream *stream = opaque;
    struct receiver_state *state = &stream->receiver_state;

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
        ssize_t r = net_recv_all(stream->socket, header, HEADER_SIZE);
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

    if (buf_size > state->remaining) {
        buf_size = state->remaining;
    }

    ssize_t r = net_recv(stream->socket, buf, buf_size);
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

static int
read_raw_packet(void *opaque, uint8_t *buf, int buf_size) {
    struct stream *stream = opaque;
    ssize_t r = net_recv(stream->socket, buf, buf_size);
    if (r == -1) {
        return AVERROR(errno);
    }
    if (r == 0) {
        return AVERROR_EOF;
    }
    return r;
}

static void
notify_stopped(void) {
    SDL_Event stop_event;
    stop_event.type = EVENT_STREAM_STOPPED;
    SDL_PushEvent(&stop_event);
}

static int
run_stream(void *data) {
    struct stream *stream = data;

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        LOGC("Could not allocate format context");
        goto end;
    }

    unsigned char *buffer = av_malloc(BUFSIZE);
    if (!buffer) {
        LOGC("Could not allocate buffer");
        goto finally_free_format_ctx;
    }

    // initialize the receiver state
    stream->receiver_state.frame_meta_queue = NULL;
    stream->receiver_state.remaining = 0;

    // if recording is enabled, a "header" is sent between raw packets
    int (*read_packet)(void *, uint8_t *, int) =
            stream->recorder ? read_packet_with_meta : read_raw_packet;
    AVIOContext *avio_ctx = avio_alloc_context(buffer, BUFSIZE, 0, stream,
                                               read_packet, NULL, NULL);
    if (!avio_ctx) {
        LOGC("Could not allocate avio context");
        // avformat_open_input takes ownership of 'buffer'
        // so only free the buffer before avformat_open_input()
        av_free(buffer);
        goto finally_free_format_ctx;
    }

    format_ctx->pb = avio_ctx;

    if (avformat_open_input(&format_ctx, NULL, NULL, NULL) < 0) {
        LOGE("Could not open video stream");
        goto finally_free_avio_ctx;
    }

    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOGE("H.264 decoder not found");
        goto end;
    }

    if (stream->decoder && !decoder_open(stream->decoder, codec)) {
        LOGE("Could not open decoder");
        goto finally_close_input;
    }

    if (stream->recorder && !recorder_open(stream->recorder, codec)) {
        LOGE("Could not open recorder");
        goto finally_close_input;
    }

    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    while (!av_read_frame(format_ctx, &packet)) {
        if (stream->decoder && !decoder_push(stream->decoder, &packet)) {
            av_packet_unref(&packet);
            goto quit;
        }

        if (stream->recorder) {
            // we retrieve the PTS in order they were received, so they will
            // be assigned to the correct frame
            uint64_t pts = receiver_state_take_meta(&stream->receiver_state);
            packet.pts = pts;
            packet.dts = pts;

            // no need to rescale with av_packet_rescale_ts(), the timestamps
            // are in microseconds both in input and output
            if (!recorder_write(stream->recorder, &packet)) {
                LOGE("Could not write frame to output file");
                av_packet_unref(&packet);
                goto quit;
            }
        }

        av_packet_unref(&packet);

        if (avio_ctx->eof_reached) {
            break;
        }
    }

    LOGD("End of frames");

quit:
    if (stream->recorder) {
        recorder_close(stream->recorder);
    }
finally_close_input:
    avformat_close_input(&format_ctx);
finally_free_avio_ctx:
    av_free(avio_ctx->buffer);
    av_free(avio_ctx);
finally_free_format_ctx:
    avformat_free_context(format_ctx);
end:
    notify_stopped();
    return 0;
}

void
stream_init(struct stream *stream, socket_t socket,
            struct decoder *decoder, struct recorder *recorder) {
    stream->socket = socket;
    stream->decoder = decoder,
    stream->recorder = recorder;
}

bool
stream_start(struct stream *stream) {
    LOGD("Starting stream thread");

    stream->thread = SDL_CreateThread(run_stream, "stream", stream);
    if (!stream->thread) {
        LOGC("Could not start stream thread");
        return false;
    }
    return true;
}

void
stream_stop(struct stream *stream) {
    if (stream->decoder) {
        decoder_interrupt(stream->decoder);
    }
}

void
stream_join(struct stream *stream) {
    SDL_WaitThread(stream->thread, NULL);
}
