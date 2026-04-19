#ifndef SC_STREAM_SINK_H
#define SC_STREAM_SINK_H

#include "common.h"

#include <stdbool.h>
#include <stdatomic.h>
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

/* Per-connection client state (defined in stream_sink.c). */
struct sc_stream_sink_client;

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

    // Template format context (no pb): holds stream definitions and codec
    // parameters used to initialise a fresh context for each new connection.
    AVFormatContext *ctx;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    // set on sc_stream_sink_stop(), packet_sink close or streaming failure
    atomic_bool stopped;

    // Init-phase queues: used only until template_ready is set.
    // After that, each sc_stream_sink_client has its own queues.
    struct sc_stream_sink_queue video_queue;
    struct sc_stream_sink_queue audio_queue;

    // wake up the stream sink thread once the video or audio codec is known
    bool video_init;
    bool audio_init;

    bool audio_expects_config_packet;

    // Stream indices shared by every per-client AVFormatContext (all clients
    // copy the template streams in the same order).
    struct sc_stream_sink_stream video_stream;
    struct sc_stream_sink_stream audio_stream;

    // Set to true once codec params + extradata are applied to the template
    // context.  Before this point packets are buffered in the init-phase queues
    // above; after this point they are fanned out to active client queues.
    bool template_ready;

    // Linked list of currently active client connections (protected by mutex).
    struct sc_stream_sink_client *clients;
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
