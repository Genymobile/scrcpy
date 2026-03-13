#ifndef SC_STREAM_SINK_H
#define SC_STREAM_SINK_H

#include "common.h"

#include <stdbool.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>

#include "trait/packet_sink.h"
#include "util/thread.h"
#include "util/vecdeque.h"

struct sc_stream_sink_queue SC_VECDEQUE(AVPacket *);

struct sc_stream_sink_stream {
    int index;
    int64_t last_pts;
};

struct sc_stream_sink {
    struct sc_packet_sink video_packet_sink;
    struct sc_packet_sink audio_packet_sink;

    /* The audio flag is unprotected:
     *  - it is initialized from sc_stream_sink_init() from the main thread;
     *  - it may be reset once from the stream sink thread if the audio is
     *    disabled dynamically.
     *
     * Therefore, once the stream sink thread is started, only the stream sink
     * thread may access it without data races.
     */
    bool audio;
    bool video;

    char *url;

    AVFormatContext *ctx;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    // set on sc_stream_sink_stop(), packet_sink close or streaming failure
    bool stopped;
    struct sc_stream_sink_queue video_queue;
    struct sc_stream_sink_queue audio_queue;

    // wake up the stream sink thread once the video or audio codec is known
    bool video_init;
    bool audio_init;

    bool audio_expects_config_packet;

    struct sc_stream_sink_stream video_stream;
    struct sc_stream_sink_stream audio_stream;
};

bool
sc_stream_sink_init(struct sc_stream_sink *sink, const char *url,
                    bool video, bool audio);

bool
sc_stream_sink_start(struct sc_stream_sink *sink);

void
sc_stream_sink_stop(struct sc_stream_sink *sink);

void
sc_stream_sink_join(struct sc_stream_sink *sink);

void
sc_stream_sink_destroy(struct sc_stream_sink *sink);

#endif
