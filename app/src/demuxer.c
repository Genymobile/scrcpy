#include "demuxer.h"

#include <assert.h>
#include <libavutil/time.h>
#include <unistd.h>

#include "decoder.h"
#include "events.h"
#include "recorder.h"
#include "util/binary.h"
#include "util/log.h"

#define SC_PACKET_HEADER_SIZE 12

#define SC_PACKET_FLAG_CONFIG    (UINT64_C(1) << 63)
#define SC_PACKET_FLAG_KEY_FRAME (UINT64_C(1) << 62)

#define SC_PACKET_PTS_MASK (SC_PACKET_FLAG_KEY_FRAME - 1)

static bool
sc_demuxer_recv_packet(struct sc_demuxer *demuxer, AVPacket *packet) {
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
    //
    // The most significant bits of the PTS are used for packet flags:
    //
    //  byte 7   byte 6   byte 5   byte 4   byte 3   byte 2   byte 1   byte 0
    // CK...... ........ ........ ........ ........ ........ ........ ........
    // ^^<------------------------------------------------------------------->
    // ||                                PTS
    // | `- key frame
    //  `-- config packet

    uint8_t header[SC_PACKET_HEADER_SIZE];
    ssize_t r = net_recv_all(demuxer->socket, header, SC_PACKET_HEADER_SIZE);
    if (r < SC_PACKET_HEADER_SIZE) {
        return false;
    }

    uint64_t pts_flags = sc_read64be(header);
    uint32_t len = sc_read32be(&header[8]);
    assert(len);

    if (av_new_packet(packet, len)) {
        LOG_OOM();
        return false;
    }

    r = net_recv_all(demuxer->socket, packet->data, len);
    if (r < 0 || ((uint32_t) r) < len) {
        av_packet_unref(packet);
        return false;
    }

    if (pts_flags & SC_PACKET_FLAG_CONFIG) {
        packet->pts = AV_NOPTS_VALUE;
    } else {
        packet->pts = pts_flags & SC_PACKET_PTS_MASK;
    }

    if (pts_flags & SC_PACKET_FLAG_KEY_FRAME) {
        packet->flags |= AV_PKT_FLAG_KEY;
    }

    packet->dts = packet->pts;
    return true;
}

static bool
push_packet_to_sinks(struct sc_demuxer *demuxer, const AVPacket *packet) {
    for (unsigned i = 0; i < demuxer->sink_count; ++i) {
        struct sc_packet_sink *sink = demuxer->sinks[i];
        if (!sink->ops->push(sink, packet)) {
            LOGE("Could not send config packet to sink %d", i);
            return false;
        }
    }

    return true;
}

static bool
sc_demuxer_push_packet(struct sc_demuxer *demuxer, AVPacket *packet) {
    bool is_config = packet->pts == AV_NOPTS_VALUE;

    // A config packet must not be decoded immediately (it contains no
    // frame); instead, it must be concatenated with the future data packet.
    if (demuxer->pending || is_config) {
        size_t offset;
        if (demuxer->pending) {
            offset = demuxer->pending->size;
            if (av_grow_packet(demuxer->pending, packet->size)) {
                LOG_OOM();
                return false;
            }
        } else {
            offset = 0;
            demuxer->pending = av_packet_alloc();
            if (!demuxer->pending) {
                LOG_OOM();
                return false;
            }
            if (av_new_packet(demuxer->pending, packet->size)) {
                LOG_OOM();
                av_packet_free(&demuxer->pending);
                return false;
            }
        }

        memcpy(demuxer->pending->data + offset, packet->data, packet->size);

        if (!is_config) {
            // prepare the concat packet to send to the decoder
            demuxer->pending->pts = packet->pts;
            demuxer->pending->dts = packet->dts;
            demuxer->pending->flags = packet->flags;
            packet = demuxer->pending;
        }
    }

    bool ok = push_packet_to_sinks(demuxer, packet);

    if (!is_config && demuxer->pending) {
        // the pending packet must be discarded (consumed or error)
        av_packet_free(&demuxer->pending);
    }

    if (!ok) {
        LOGE("Could not process packet");
        return false;
    }

    return true;
}

static void
sc_demuxer_close_first_sinks(struct sc_demuxer *demuxer, unsigned count) {
    while (count) {
        struct sc_packet_sink *sink = demuxer->sinks[--count];
        sink->ops->close(sink);
    }
}

static inline void
sc_demuxer_close_sinks(struct sc_demuxer *demuxer) {
    sc_demuxer_close_first_sinks(demuxer, demuxer->sink_count);
}

static bool
sc_demuxer_open_sinks(struct sc_demuxer *demuxer, const AVCodec *codec) {
    for (unsigned i = 0; i < demuxer->sink_count; ++i) {
        struct sc_packet_sink *sink = demuxer->sinks[i];
        if (!sink->ops->open(sink, codec)) {
            LOGE("Could not open packet sink %d", i);
            sc_demuxer_close_first_sinks(demuxer, i);
            return false;
        }
    }

    return true;
}

static int
run_demuxer(void *data) {
    struct sc_demuxer *demuxer = data;

    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOGE("H.264 decoder not found");
        goto end;
    }

    demuxer->codec_ctx = avcodec_alloc_context3(codec);
    if (!demuxer->codec_ctx) {
        LOG_OOM();
        goto end;
    }

    if (!sc_demuxer_open_sinks(demuxer, codec)) {
        LOGE("Could not open demuxer sinks");
        goto finally_free_codec_ctx;
    }

    demuxer->parser = av_parser_init(AV_CODEC_ID_H264);
    if (!demuxer->parser) {
        LOGE("Could not initialize parser");
        goto finally_close_sinks;
    }

    // We must only pass complete frames to av_parser_parse2()!
    // It's more complicated, but this allows to reduce the latency by 1 frame!
    demuxer->parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOG_OOM();
        goto finally_close_parser;
    }

    for (;;) {
        bool ok = sc_demuxer_recv_packet(demuxer, packet);
        if (!ok) {
            // end of stream
            break;
        }

        ok = sc_demuxer_push_packet(demuxer, packet);
        av_packet_unref(packet);
        if (!ok) {
            // cannot process packet (error already logged)
            break;
        }
    }

    LOGD("End of frames");

    if (demuxer->pending) {
        av_packet_free(&demuxer->pending);
    }

    av_packet_free(&packet);
finally_close_parser:
    av_parser_close(demuxer->parser);
finally_close_sinks:
    sc_demuxer_close_sinks(demuxer);
finally_free_codec_ctx:
    avcodec_free_context(&demuxer->codec_ctx);
end:
    demuxer->cbs->on_eos(demuxer, demuxer->cbs_userdata);

    return 0;
}

void
sc_demuxer_init(struct sc_demuxer *demuxer, sc_socket socket,
                const struct sc_demuxer_callbacks *cbs, void *cbs_userdata) {
    demuxer->socket = socket;
    demuxer->pending = NULL;
    demuxer->sink_count = 0;

    assert(cbs && cbs->on_eos);

    demuxer->cbs = cbs;
    demuxer->cbs_userdata = cbs_userdata;
}

void
sc_demuxer_add_sink(struct sc_demuxer *demuxer, struct sc_packet_sink *sink) {
    assert(demuxer->sink_count < SC_DEMUXER_MAX_SINKS);
    assert(sink);
    assert(sink->ops);
    demuxer->sinks[demuxer->sink_count++] = sink;
}

bool
sc_demuxer_start(struct sc_demuxer *demuxer) {
    LOGD("Starting demuxer thread");

    bool ok = sc_thread_create(&demuxer->thread, run_demuxer, "scrcpy-demuxer",
                               demuxer);
    if (!ok) {
        LOGE("Could not start demuxer thread");
        return false;
    }
    return true;
}

void
sc_demuxer_join(struct sc_demuxer *demuxer) {
    sc_thread_join(&demuxer->thread, NULL);
}
