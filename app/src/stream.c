#include "stream.h"

#include <assert.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <unistd.h>

#include "decoder.h"
#include "events.h"
#include "recorder.h"
#include "util/buffer_util.h"
#include "util/log.h"

#define BUFSIZE 0x10000

#define HEADER_SIZE 12
#define NO_PTS UINT64_C(-1)

static bool
stream_recv_packet(struct stream *stream, AVPacket *packet) {
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

    uint8_t header[HEADER_SIZE];
    ssize_t r = net_recv_all(stream->socket, header, HEADER_SIZE);
    if (r < HEADER_SIZE) {
        return false;
    }

    uint64_t pts = buffer_read64be(header);
    uint32_t len = buffer_read32be(&header[8]);
    assert(pts == NO_PTS || (pts & 0x8000000000000000) == 0);
    assert(len);

    if (av_new_packet(packet, len)) {
        LOGE("Could not allocate packet");
        return false;
    }

    r = net_recv_all(stream->socket, packet->data, len);
    if (r < 0 || ((uint32_t) r) < len) {
        av_packet_unref(packet);
        return false;
    }

    packet->pts = pts != NO_PTS ? (int64_t) pts : AV_NOPTS_VALUE;

    return true;
}

static bool
push_packet_to_sinks(struct stream *stream, const AVPacket *packet) {
    for (unsigned i = 0; i < stream->sink_count; ++i) {
        struct sc_packet_sink *sink = stream->sinks[i];
        if (!sink->ops->push(sink, packet)) {
            LOGE("Could not send config packet to sink %d", i);
            return false;
        }
    }

    return true;
}

static bool
stream_parse(struct stream *stream, AVPacket *packet) {
    uint8_t *in_data = packet->data;
    int in_len = packet->size;
    uint8_t *out_data = NULL;
    int out_len = 0;
    int r = av_parser_parse2(stream->parser, stream->codec_ctx,
                             &out_data, &out_len, in_data, in_len,
                             AV_NOPTS_VALUE, AV_NOPTS_VALUE, -1);

    // PARSER_FLAG_COMPLETE_FRAMES is set
    assert(r == in_len);
    (void) r;
    assert(out_len == in_len);

    if (stream->parser->key_frame == 1) {
        packet->flags |= AV_PKT_FLAG_KEY;
    }

    packet->dts = packet->pts;

    bool ok = push_packet_to_sinks(stream, packet);
    if (!ok) {
        LOGE("Could not process packet");
        return false;
    }

    return true;
}

static bool
stream_push_packet(struct stream *stream, AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;

    // A config packet must not be decoded immediately (it contains no
    // frame); instead, it must be concatenated with the future data packet.
    if (stream->pending || is_config) {
        size_t offset;
        if (stream->pending) {
            offset = stream->pending->size;
            if (av_grow_packet(stream->pending, packet->size)) {
                LOGE("Could not grow packet");
                return false;
            }
        } else {
            offset = 0;
            stream->pending = av_packet_alloc();
            if (!stream->pending) {
                LOGE("Could not allocate packet");
                return false;
            }
            if (av_new_packet(stream->pending, packet->size)) {
                LOGE("Could not create packet");
                av_packet_free(&stream->pending);
                return false;
            }
        }

        memcpy(stream->pending->data + offset, packet->data, packet->size);

        if (!is_config) {
            // prepare the concat packet to send to the decoder
            stream->pending->pts = packet->pts;
            stream->pending->dts = packet->dts;
            stream->pending->flags = packet->flags;
            packet = stream->pending;
        }
    }

    if (is_config) {
        // config packet
        bool ok = push_packet_to_sinks(stream, packet);
        if (!ok) {
            return false;
        }
    } else {
        // data packet
        bool ok = stream_parse(stream, packet);

        if (stream->pending) {
            // the pending packet must be discarded (consumed or error)
            av_packet_free(&stream->pending);
        }

        if (!ok) {
            return false;
        }
    }
    return true;
}

static void
stream_close_first_sinks(struct stream *stream, unsigned count) {
    while (count) {
        struct sc_packet_sink *sink = stream->sinks[--count];
        sink->ops->close(sink);
    }
}

static inline void
stream_close_sinks(struct stream *stream) {
    stream_close_first_sinks(stream, stream->sink_count);
}

static bool
stream_open_sinks(struct stream *stream, const AVCodec *codec) {
    for (unsigned i = 0; i < stream->sink_count; ++i) {
        struct sc_packet_sink *sink = stream->sinks[i];
        if (!sink->ops->open(sink, codec)) {
            LOGE("Could not open packet sink %d", i);
            stream_close_first_sinks(stream, i);
            return false;
        }
    }

    return true;
}

static int
run_stream(void *data) {
    struct stream *stream = data;

    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOGE("H.264 decoder not found");
        goto end;
    }

    stream->codec_ctx = avcodec_alloc_context3(codec);
    if (!stream->codec_ctx) {
        LOGC("Could not allocate codec context");
        goto end;
    }

    if (!stream_open_sinks(stream, codec)) {
        LOGE("Could not open stream sinks");
        goto finally_free_codec_ctx;
    }

    stream->parser = av_parser_init(AV_CODEC_ID_H264);
    if (!stream->parser) {
        LOGE("Could not initialize parser");
        goto finally_close_sinks;
    }

    // We must only pass complete frames to av_parser_parse2()!
    // It's more complicated, but this allows to reduce the latency by 1 frame!
    stream->parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOGE("Could not allocate packet");
        goto finally_close_parser;
    }

    for (;;) {
        bool ok = stream_recv_packet(stream, packet);
        if (!ok) {
            // end of stream
            break;
        }

        ok = stream_push_packet(stream, packet);
        av_packet_unref(packet);
        if (!ok) {
            // cannot process packet (error already logged)
            break;
        }
    }

    LOGD("End of frames");

    if (stream->pending) {
        av_packet_free(&stream->pending);
    }

    av_packet_free(&packet);
finally_close_parser:
    av_parser_close(stream->parser);
finally_close_sinks:
    stream_close_sinks(stream);
finally_free_codec_ctx:
    avcodec_free_context(&stream->codec_ctx);
end:
    stream->cbs->on_eos(stream, stream->cbs_userdata);

    return 0;
}

void
stream_init(struct stream *stream, socket_t socket,
            const struct stream_callbacks *cbs, void *cbs_userdata) {
    stream->socket = socket;
    stream->pending = NULL;
    stream->sink_count = 0;

    assert(cbs && cbs->on_eos);

    stream->cbs = cbs;
    stream->cbs_userdata = cbs_userdata;
}

void
stream_add_sink(struct stream *stream, struct sc_packet_sink *sink) {
    assert(stream->sink_count < STREAM_MAX_SINKS);
    assert(sink);
    assert(sink->ops);
    stream->sinks[stream->sink_count++] = sink;
}

bool
stream_start(struct stream *stream) {
    LOGD("Starting stream thread");

    bool ok = sc_thread_create(&stream->thread, run_stream, "stream", stream);
    if (!ok) {
        LOGC("Could not start stream thread");
        return false;
    }
    return true;
}

void
stream_join(struct stream *stream) {
    sc_thread_join(&stream->thread, NULL);
}
