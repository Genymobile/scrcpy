#include "stream_sink.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

#include "util/log.h"

/** Downcast packet sinks to stream sink */
#define DOWNCAST_VIDEO(SINK) \
    container_of(SINK, struct sc_stream_sink, video_packet_sink)
#define DOWNCAST_AUDIO(SINK) \
    container_of(SINK, struct sc_stream_sink, audio_packet_sink)

static const AVRational SCRCPY_TIME_BASE = {1, 1000000}; // timestamps in us

/** Arbitrary duration assigned to the last video packet (0.1 s in µs). */
#define LAST_VIDEO_PACKET_DURATION_US 100000

/** Return true if `key=` appears in the URL's query string. */
static bool
sc_url_has_param(const char *url, const char *key) {
    const char *q = strchr(url, '?');
    if (!q) {
        return false;
    }
    size_t klen = strlen(key);
    const char *p = q + 1;
    while (*p) {
        if (!strncmp(p, key, klen) && p[klen] == '=') {
            return true;
        }
        const char *amp = strchr(p, '&');
        if (!amp) {
            break;
        }
        p = amp + 1;
    }
    return false;
}

/** Append "key=value" to url. Returns a newly allocated string. */
static char *
sc_url_append_param(const char *url, const char *key, const char *value) {
    const char *sep = strchr(url, '?') ? "&" : "?";
    size_t len = strlen(url) + strlen(sep) + strlen(key) + 1 /* '=' */
                 + strlen(value) + 1 /* '\0' */;
    char *result = malloc(len);
    if (!result) {
        LOG_OOM();
        return NULL;
    }
    snprintf(result, len, "%s%s%s=%s", url, sep, key, value);
    return result;
}

/**
 * Build the connect URL for the stream sink.
 *
 * For known protocols:
 *  - srt://  adds ?mode=listener and ?latency=50 (ms) if not already set
 *            (override with ?latency=200 or higher for WAN links)
 *  - tcp://  adds ?listen=1 if not already set
 *  - udp://, rtp://  connectionless; returned as-is
 * Unknown protocols emit a warning and are returned as-is.
 *
 * Returns a newly allocated string; the caller must free it.
 */
static char *
sc_stream_sink_build_connect_url(const char *url) {
    bool is_srt = !strncmp(url, "srt://", 6);
    bool is_tcp = !strncmp(url, "tcp://", 6);
    bool is_udp = !strncmp(url, "udp://", 6);
    bool is_rtp = !strncmp(url, "rtp://", 6);

    if (!is_srt && !is_tcp && !is_udp && !is_rtp) {
        LOGW("Stream sink: unrecognized protocol in \"%s\"; "
             "no listener mode or latency tuning applied", url);
        return strdup(url);
    }

    char *result = strdup(url);
    if (!result) {
        LOG_OOM();
        return NULL;
    }

    if (is_srt) {
        // scrcpy acts as the SRT listener (server) by default
        if (!sc_url_has_param(result, "mode")) {
            char *tmp = sc_url_append_param(result, "mode", "listener");
            free(result);
            if (!tmp) {
                return NULL;
            }
            result = tmp;
        }
    } else if (is_tcp) {
        // scrcpy acts as the TCP server, waiting for a player to connect
        if (!sc_url_has_param(result, "listen")) {
            char *tmp = sc_url_append_param(result, "listen", "1");
            free(result);
            if (!tmp) {
                return NULL;
            }
            result = tmp;
        }
    }
    // udp:// is connectionless; no listener mode needed

    return result;
}

/**
 * Check if a URL uses a connectionless protocol (UDP).
 *
 * For this protocol, only a single output stream is needed,
 * not multiple client connections.
 */
static inline bool
sc_stream_sink_is_connectionless(const char *url) {
    return !strncmp(url, "udp://", 6);
}

static AVPacket *
sc_stream_sink_packet_ref(const AVPacket *packet) {
    AVPacket *p = av_packet_alloc();
    if (!p) {
        LOG_OOM();
        return NULL;
    }

    if (av_packet_ref(p, packet)) {
        av_packet_free(&p);
        return NULL;
    }

    return p;
}

static void
sc_stream_sink_queue_clear(struct sc_stream_sink_queue *queue) {
    while (!sc_vecdeque_is_empty(queue)) {
        AVPacket *p = sc_vecdeque_pop(queue);
        av_packet_free(&p);
    }
}

static bool
sc_stream_sink_set_extradata(AVStream *ostream, const AVPacket *packet) {
    uint8_t *extradata = av_malloc(packet->size * sizeof(uint8_t));
    if (!extradata) {
        LOG_OOM();
        return false;
    }

    // copy the first packet to the extra data
    memcpy(extradata, packet->data, packet->size);

    ostream->codecpar->extradata = extradata;
    ostream->codecpar->extradata_size = packet->size;
    return true;
}

/**
 * Per-connection state for one active streaming client.
 *
 * All fields that require synchronisation are protected by the parent
 * sc_stream_sink::mutex / sc_stream_sink::cond.  Fields only accessed from
 * the client's own thread (video/audio stream PTS tracking) need no locking.
 */
struct sc_stream_sink_client {
    struct sc_stream_sink *sink;  /**< back-pointer to parent */
    AVFormatContext *ctx;         /**< own output context (with pb) */
    sc_thread thread;

    /** Request the write loop to stop (protected by sink->mutex). */
    bool stopped;
    /** The thread has exited; safe to join (protected by sink->mutex). */
    bool finished;

    /** Per-client packet queues (protected by sink->mutex). */
    struct sc_stream_sink_queue video_queue;
    struct sc_stream_sink_queue audio_queue;

    /** PTS tracking – only accessed from the client thread, no mutex needed. */
    struct sc_stream_sink_stream video_stream;
    struct sc_stream_sink_stream audio_stream;

    /** Intrusive linked list, protected by sink->mutex. */
    struct sc_stream_sink_client *next;
};

/**
 * Allocate a new AVFormatContext initialised from the template context.
 *
 * Copies all streams and their codec parameters (including extradata) so that
 * the returned context is ready to have a pb attached and a stream header
 * written.  Returns NULL on allocation failure.
 */
static AVFormatContext *
sc_stream_sink_create_client_ctx(const struct sc_stream_sink *sink) {
    const AVOutputFormat *oformat = av_guess_format("mpegts", NULL, NULL);
    assert(oformat); // already verified in sc_stream_sink_init

    AVFormatContext *ctx = avformat_alloc_context();
    if (!ctx) {
        LOG_OOM();
        return NULL;
    }

    // See sc_stream_sink_init for the rationale behind these flags.
    ctx->oformat = (AVOutputFormat *) oformat;
    ctx->flags |= AVFMT_FLAG_FLUSH_PACKETS;
    ctx->max_interleave_delta = AV_TIME_BASE / 10; // 100 ms

    for (unsigned i = 0; i < sink->ctx->nb_streams; i++) {
        AVStream *src = sink->ctx->streams[i];
        AVStream *dst = avformat_new_stream(ctx, NULL);
        if (!dst) {
            avformat_free_context(ctx);
            return NULL;
        }
        if (avcodec_parameters_copy(dst->codecpar, src->codecpar) < 0) {
            avformat_free_context(ctx);
            return NULL;
        }
        dst->time_base = src->time_base;
    }

    return ctx;
}

static inline void
sc_stream_sink_rescale_packet(AVStream *stream, AVPacket *packet) {
    av_packet_rescale_ts(packet, SCRCPY_TIME_BASE, stream->time_base);
}

static bool
sc_stream_sink_write_stream(struct sc_stream_sink_client *client,
                            struct sc_stream_sink_stream *st,
                            AVPacket *packet) {
    AVStream *stream = client->ctx->streams[st->index];
    sc_stream_sink_rescale_packet(stream, packet);
    if (st->last_pts != AV_NOPTS_VALUE && packet->pts <= st->last_pts) {
        LOGD("Fixing PTS non monotonically increasing in stream %d "
             "(%" PRIi64 " >= %" PRIi64 ")",
             st->index, st->last_pts, packet->pts);
        packet->pts = ++st->last_pts;
        packet->dts = packet->pts;
    } else {
        st->last_pts = packet->pts;
    }
    return av_interleaved_write_frame(client->ctx, packet) >= 0;
}

static inline bool
sc_stream_sink_write_video(struct sc_stream_sink_client *client,
                           AVPacket *packet) {
    return sc_stream_sink_write_stream(client, &client->video_stream, packet);
}

static inline bool
sc_stream_sink_write_audio(struct sc_stream_sink_client *client,
                           AVPacket *packet) {
    return sc_stream_sink_write_stream(client, &client->audio_stream, packet);
}

static int
sc_stream_sink_interrupt_cb(void *data) {
    struct sc_stream_sink *sink = data;
    // Read without mutex: this is intentional (same pattern as interrupt
    // callbacks in other parts of the codebase)
    return sink->stopped ? 1 : 0;
}

/** Interrupt callback for a per-client AVFormatContext. */
static int
sc_stream_sink_client_interrupt_cb(void *data) {
    struct sc_stream_sink_client *client = data;
    // Read without mutex: intentional (benign race; same pattern as above)
    return (client->stopped || client->sink->stopped) ? 1 : 0;
}

static inline bool
sc_stream_sink_must_wait_for_config_packets(struct sc_stream_sink *sink) {
    if (sink->video && sc_vecdeque_is_empty(&sink->video_queue)) {
        // The video queue is empty
        return true;
    }

    if (sink->audio && sink->audio_expects_config_packet
            && sc_vecdeque_is_empty(&sink->audio_queue)) {
        // The audio queue is empty (when audio is enabled)
        return true;
    }

    // No queue is empty
    return false;
}

/**
 * Wait for codec initialisation and the first config packets, then apply the
 * codec parameters and extradata to the template AVFormatContext
 * (sink->ctx).  Does NOT open any network connection.
 *
 * On success, sink->template_ready is set to true and the init-phase queues
 * are cleared.  Returns false only when the sink is stopped before init
 * completes.
 */
static bool
sc_stream_sink_init_template(struct sc_stream_sink *sink) {
    sc_mutex_lock(&sink->mutex);

    while (!sink->stopped &&
              ((sink->video && !sink->video_init)
            || (sink->audio && !sink->audio_init)
            || sc_stream_sink_must_wait_for_config_packets(sink))) {
        sc_cond_wait(&sink->cond, &sink->mutex);
    }

    if (sink->video && sc_vecdeque_is_empty(&sink->video_queue)) {
        assert(sink->stopped);
        sc_mutex_unlock(&sink->mutex);
        return false;
    }

    AVPacket *video_pkt = NULL;
    if (!sc_vecdeque_is_empty(&sink->video_queue)) {
        assert(sink->video);
        video_pkt = sc_vecdeque_pop(&sink->video_queue);
    }

    AVPacket *audio_pkt = NULL;
    if (sink->audio_expects_config_packet &&
            !sc_vecdeque_is_empty(&sink->audio_queue)) {
        assert(sink->audio);
        audio_pkt = sc_vecdeque_pop(&sink->audio_queue);
    }

    sc_mutex_unlock(&sink->mutex);

    bool ret = false;

    if (video_pkt) {
        if (video_pkt->pts != AV_NOPTS_VALUE) {
            LOGE("The first video packet is not a config packet");
            goto end;
        }

        assert(sink->video_stream.index >= 0);
        AVStream *video_stream = sink->ctx->streams[sink->video_stream.index];
        bool ok = sc_stream_sink_set_extradata(video_stream, video_pkt);
        if (!ok) {
            goto end;
        }
    }

    if (audio_pkt) {
        if (audio_pkt->pts != AV_NOPTS_VALUE) {
            LOGE("The first audio packet is not a config packet");
            goto end;
        }

        assert(sink->audio_stream.index >= 0);
        AVStream *audio_stream = sink->ctx->streams[sink->audio_stream.index];
        bool ok = sc_stream_sink_set_extradata(audio_stream, audio_pkt);
        if (!ok) {
            goto end;
        }
    }

    ret = true;

end:
    av_packet_free(&video_pkt);
    av_packet_free(&audio_pkt);

    if (ret) {
        // Atomically mark the template as ready and discard any packets that
        // accumulated in the init-phase queues during the wait above.
        // From this point push() fans out directly to per-client queues.
        sc_mutex_lock(&sink->mutex);
        sink->template_ready = true;
        sc_stream_sink_queue_clear(&sink->video_queue);
        sc_stream_sink_queue_clear(&sink->audio_queue);
        sc_mutex_unlock(&sink->mutex);
    }

    return ret;
}

/**
 * Per-client packet write loop.
 *
 * Dequeues packets from the client's own video/audio queues (which receive
 * fan-out copies from the push callbacks) and writes them to client->ctx->pb.
 * Returns false only when a write error occurs (i.e. the client disconnected);
 * returns true when stopped cleanly.
 */
static bool
sc_stream_sink_client_run_stream(struct sc_stream_sink_client *client) {
    struct sc_stream_sink *sink = client->sink;

    int64_t pts_origin = AV_NOPTS_VALUE;

    AVPacket *video_pkt = NULL;
    AVPacket *audio_pkt = NULL;

    // Buffer the previous video packet until the next one arrives so we can
    // compute its duration.
    AVPacket *video_pkt_previous = NULL;

    bool error = false;

    for (;;) {
        sc_mutex_lock(&sink->mutex);

        while (!client->stopped) {
            if (sink->video && !video_pkt &&
                    !sc_vecdeque_is_empty(&client->video_queue)) {
                break;
            }
            if (sink->audio && !audio_pkt &&
                    !sc_vecdeque_is_empty(&client->audio_queue)) {
                break;
            }
            sc_cond_wait(&sink->cond, &sink->mutex);
        }

        // client->stopped may now be set; drain remaining packets before exit.

        assert(sink->video || (!video_pkt
                && sc_vecdeque_is_empty(&client->video_queue)));
        assert(sink->audio || (!audio_pkt
                && sc_vecdeque_is_empty(&client->audio_queue)));

        if (!video_pkt && !sc_vecdeque_is_empty(&client->video_queue)) {
            video_pkt = sc_vecdeque_pop(&client->video_queue);
        }

        if (!audio_pkt && !sc_vecdeque_is_empty(&client->audio_queue)) {
            audio_pkt = sc_vecdeque_pop(&client->audio_queue);
        }

        if (client->stopped && !video_pkt && !audio_pkt) {
            sc_mutex_unlock(&sink->mutex);
            break;
        }

        assert(video_pkt || audio_pkt);

        sc_mutex_unlock(&sink->mutex);

        // Discard further config packets (e.g. on device orientation change).
        if (video_pkt && video_pkt->pts == AV_NOPTS_VALUE) {
            av_packet_free(&video_pkt);
            video_pkt = NULL;
        }

        if (audio_pkt && audio_pkt->pts == AV_NOPTS_VALUE) {
            av_packet_free(&audio_pkt);
            audio_pkt = NULL;
        }

        if (pts_origin == AV_NOPTS_VALUE) {
            if (!sink->audio) {
                assert(video_pkt);
                pts_origin = video_pkt->pts;
            } else if (!sink->video) {
                assert(audio_pkt);
                pts_origin = audio_pkt->pts;
            } else if (video_pkt && audio_pkt) {
                pts_origin = MIN(video_pkt->pts, audio_pkt->pts);
            } else if (client->stopped) {
                if (video_pkt) {
                    pts_origin = video_pkt->pts;
                } else {
                    // Stopped without any usable video: nothing to stream.
                    goto end;
                }
            } else {
                // Need both video and audio to initialise pts_origin.
                continue;
            }
        }

        assert(pts_origin != AV_NOPTS_VALUE);

        if (video_pkt) {
            video_pkt->pts -= pts_origin;
            video_pkt->dts = video_pkt->pts;

            if (video_pkt_previous) {
                video_pkt_previous->duration =
                    video_pkt->pts - video_pkt_previous->pts;

                bool ok = sc_stream_sink_write_video(client, video_pkt_previous);
                av_packet_free(&video_pkt_previous);
                if (!ok) {
                    LOGD("Stream sink: client disconnected (video write error)");
                    error = true;
                    goto end;
                }
            }

            video_pkt_previous = video_pkt;
            video_pkt = NULL;
        }

        if (audio_pkt) {
            audio_pkt->pts -= pts_origin;
            audio_pkt->dts = audio_pkt->pts;

            bool ok = sc_stream_sink_write_audio(client, audio_pkt);
            av_packet_free(&audio_pkt);
            audio_pkt = NULL;
            if (!ok) {
                LOGD("Stream sink: client disconnected (audio write error)");
                error = true;
                goto end;
            }
        }
    }

    // Write the last video packet.
    if (video_pkt_previous) {
        video_pkt_previous->duration = LAST_VIDEO_PACKET_DURATION_US;
        bool ok = sc_stream_sink_write_video(client, video_pkt_previous);
        if (!ok) {
            LOGW("Stream sink: could not write last video packet");
        }
        av_packet_free(&video_pkt_previous);
        video_pkt_previous = NULL;
    }

    av_write_trailer(client->ctx);

end:
    av_packet_free(&video_pkt);
    av_packet_free(&audio_pkt);
    av_packet_free(&video_pkt_previous);

    return !error;
}

/**
 * Thread function for a per-client connection.
 *
 * Writes packets until the client disconnects or the sink is stopped, then
 * closes the connection and marks itself as finished so the accept loop can
 * reap it.
 */
static int
run_stream_sink_client(void *data) {
    struct sc_stream_sink_client *client = data;
    struct sc_stream_sink *sink = client->sink;

    sc_stream_sink_client_run_stream(client);

    // WORKAROUND: SRT epoll deadlock on disconnect
    // When closing SRT sockets, FFmpeg's interrupt callback and SRT's internal
    // epoll management conflict, causing "no sockets to check, this would deadlock".
    // Root cause: FFmpeg may call interrupt_callback during avio_close(), but SRT
    // has already removed the socket from epoll, causing state inconsistency.
    // TODO: Remove this workaround once SRT/FFmpeg fix the socket lifecycle interaction.
    // For now, only skip avio_close() for SRT; other protocols are safe.
    bool is_srt = sink->url && !strncmp(sink->url, "srt://", 6);

    if (client->ctx->pb) {
        if (is_srt) {
            // SRT workaround: don't call avio_close(), let avformat_free_context() handle it
            client->ctx->pb = NULL;
        } else {
            // Safe for TCP, UDP and other protocols
            avio_close(client->ctx->pb);
            client->ctx->pb = NULL;
        }
    }

    // Mark as finished so the accept loop can join and free us.
    sc_mutex_lock(&sink->mutex);
    client->finished = true;
    // Drain any packets that arrived between the write error and now.
    sc_stream_sink_queue_clear(&client->video_queue);
    sc_stream_sink_queue_clear(&client->audio_queue);
    sc_mutex_unlock(&sink->mutex);

    LOGD("Stream sink: client thread ended");

    return 0;
}

/**
 * Join and free all client threads that have set finished=true.
 * Must be called from the accept loop (main sink thread).
 */
static void
sc_stream_sink_reap_dead_clients(struct sc_stream_sink *sink) {
    struct sc_stream_sink_client *dead = NULL;

    sc_mutex_lock(&sink->mutex);
    struct sc_stream_sink_client **pp = &sink->clients;
    while (*pp) {
        struct sc_stream_sink_client *c = *pp;
        if (c->finished) {
            *pp = c->next; // remove from list
            c->next = dead;
            dead = c;
        } else {
            pp = &c->next;
        }
    }
    sc_mutex_unlock(&sink->mutex);

    while (dead) {
        struct sc_stream_sink_client *c = dead;
        dead = c->next;
        sc_thread_join(&c->thread, NULL);
        avformat_free_context(c->ctx);
        sc_vecdeque_destroy(&c->video_queue);
        sc_vecdeque_destroy(&c->audio_queue);
        free(c);
    }
}

/**
 * Main streaming loop: initialises the template context once, then either:
 *  - For connection-oriented protocols (TCP, SRT): repeatedly accepts incoming
 *    connections, spawning a per-client thread for each.
 *  - For connectionless protocols (UDP, RTP): creates a single output stream
 *    and writes all packets to it directly.
 * Runs until sink->stopped is set (by sc_stream_sink_stop() or device EOS).
 */

// Forward declaration: defined below alongside the other packet-sink callbacks.
static void sc_stream_sink_stream_init(struct sc_stream_sink_stream *stream);

static int
run_stream_sink(void *data) {
    struct sc_stream_sink *sink = data;

    bool ok = sc_stream_sink_init_template(sink);
    if (!ok) {
        LOGE("Stream sink: initialisation failed");
        goto stop;
    }

    bool is_connectionless = sc_stream_sink_is_connectionless(sink->url);

    char *connect_url = sc_stream_sink_build_connect_url(sink->url);
    if (!connect_url) {
        goto stop;
    }

    if (is_connectionless) {
        LOGI("Stream sink: streaming to %s", connect_url);
    } else {
        LOGI("Stream sink: listening for clients on %s", connect_url);
    }

    bool connectionless_done = false;

    while (!sink->stopped) {
        // For connectionless protocols, only attempt one connection
        if (is_connectionless && connectionless_done) {
            // Keep the single client thread running; just wait until stopped
            sc_mutex_lock(&sink->mutex);
            while (!sink->stopped) {
                sc_cond_wait(&sink->cond, &sink->mutex);
            }
            sc_mutex_unlock(&sink->mutex);
            break;
        }

        // Reap any client threads that finished since the last iteration.
        sc_stream_sink_reap_dead_clients(sink);
        AVIOInterruptCB int_cb = {
            .callback = sc_stream_sink_interrupt_cb,
            .opaque = sink,
        };

        // Block here until one client connects (or sink is stopped).
        AVIOContext *pb = NULL;
        int r = avio_open2(&pb, connect_url, AVIO_FLAG_WRITE, &int_cb, NULL);
        if (r < 0) {
            if (!sink->stopped) {
                LOGE("Stream sink: failed to accept connection on %s",
                     sink->url);
            }
            break;
        }

        // Build a fresh output context for this client from the template.
        AVFormatContext *client_ctx = sc_stream_sink_create_client_ctx(sink);
        if (!client_ctx) {
            avio_close(pb);
            continue;
        }
        client_ctx->pb = pb;

        // Allocate and initialise the client struct.
        struct sc_stream_sink_client *client =
            calloc(1, sizeof(struct sc_stream_sink_client));
        if (!client) {
            LOG_OOM();
            bool is_srt = sink->url && !strncmp(sink->url, "srt://", 6);
            if (!is_srt) {
                avio_close(client_ctx->pb);
            }
            client_ctx->pb = NULL;
            avformat_free_context(client_ctx);
            continue;
        }
        client->sink = sink;
        client->ctx = client_ctx;
        client->stopped = false;
        client->finished = false;
        sc_vecdeque_init(&client->video_queue);
        sc_vecdeque_init(&client->audio_queue);
        sc_stream_sink_stream_init(&client->video_stream);
        sc_stream_sink_stream_init(&client->audio_stream);
        // Copy shared stream indices so the client can look up its streams.
        client->video_stream.index = sink->video_stream.index;
        client->audio_stream.index = sink->audio_stream.index;

        // Switch the client context's interrupt callback to check client->stopped too,
        // so blocking I/O in the client thread can be interrupted on demand.
        client_ctx->interrupt_callback.callback = sc_stream_sink_client_interrupt_cb;
        client_ctx->interrupt_callback.opaque = client;

        // Write the MPEG-TS stream header for this client.
        if (avformat_write_header(client_ctx, NULL) < 0) {
            LOGE("Stream sink: failed to write stream header to client");
            client_ctx->pb = NULL;  // Don't avio_close() - causes SRT epoll issues
            avformat_free_context(client_ctx);
            free(client);
            continue;
        }

        // Add the client to the active list before spawning its thread so
        // that packets pushed between now and the thread start are queued.
        sc_mutex_lock(&sink->mutex);
        client->next = sink->clients;
        sink->clients = client;
        sc_mutex_unlock(&sink->mutex);

        bool thread_ok = sc_thread_create(&client->thread,
                                          run_stream_sink_client,
                                          "scrcpy-sclient", client);
        if (!thread_ok) {
            LOGE("Stream sink: could not create client thread");
            sc_mutex_lock(&sink->mutex);
            // Remove from list (it was just prepended)
            sink->clients = client->next;
            sc_stream_sink_queue_clear(&client->video_queue);
            sc_stream_sink_queue_clear(&client->audio_queue);
            sc_mutex_unlock(&sink->mutex);
            if (client_ctx->pb) {
                bool is_srt = sink->url && !strncmp(sink->url, "srt://", 6);
                if (!is_srt) {
                    avio_close(client_ctx->pb);
                }
                client_ctx->pb = NULL;
            }
            avformat_free_context(client_ctx);
            sc_vecdeque_destroy(&client->video_queue);
            sc_vecdeque_destroy(&client->audio_queue);
            free(client);
            continue;
        }

        LOGI("Stream sink: client connected on %s", sink->url);

        if (is_connectionless) {
            // For connectionless protocols (UDP, RTP), we only need a single
            // stream. Mark it as done and the loop will now wait instead of
            // trying to accept new connections.
            connectionless_done = true;
        }
    }

    free(connect_url);

stop:
    // Stop and drain all active clients.
    sc_mutex_lock(&sink->mutex);
    sink->stopped = true;
    struct sc_stream_sink_client *c = sink->clients;
    while (c) {
        c->stopped = true;
        c = c->next;
    }
    sc_cond_broadcast(&sink->cond);
    sc_mutex_unlock(&sink->mutex);

    // Join every remaining client thread, then free the client structs.
    sc_mutex_lock(&sink->mutex);
    struct sc_stream_sink_client *head = sink->clients;
    sink->clients = NULL;
    sc_mutex_unlock(&sink->mutex);

    while (head) {
        struct sc_stream_sink_client *next = head->next;
        sc_thread_join(&head->thread, NULL);
        if (head->ctx->pb) {
            bool is_srt = sink->url && !strncmp(sink->url, "srt://", 6);
            if (!is_srt) {
                avio_close(head->ctx->pb);
            }
            head->ctx->pb = NULL;
        }
        avformat_free_context(head->ctx);
        sc_vecdeque_destroy(&head->video_queue);
        sc_vecdeque_destroy(&head->audio_queue);
        free(head);
        head = next;
    }

    LOGD("Stream sink thread ended");

    return 0;
}

static void
sc_stream_sink_stream_init(struct sc_stream_sink_stream *stream) {
    stream->index = -1;
    stream->last_pts = AV_NOPTS_VALUE;
}

static bool
sc_stream_sink_video_packet_sink_open(struct sc_packet_sink *sink,
                                      AVCodecContext *ctx) {
    struct sc_stream_sink *ss = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(!ss->video_init);

    sc_mutex_lock(&ss->mutex);
    if (ss->stopped) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    AVStream *stream = avformat_new_stream(ss->ctx, ctx->codec);
    if (!stream) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    int r = avcodec_parameters_from_context(stream->codecpar, ctx);
    if (r < 0) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    ss->video_stream.index = stream->index;

    ss->video_init = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);

    return true;
}

static void
sc_stream_sink_video_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_stream_sink *ss = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(ss->video_init);

    sc_mutex_lock(&ss->mutex);
    // EOS also stops the stream sink
    ss->stopped = true;
    struct sc_stream_sink_client *c = ss->clients;
    while (c) {
        c->stopped = true;
        c = c->next;
    }
    sc_cond_broadcast(&ss->cond);
    sc_mutex_unlock(&ss->mutex);
}

static bool
sc_stream_sink_video_packet_sink_push(struct sc_packet_sink *sink,
                                      const AVPacket *packet) {
    struct sc_stream_sink *ss = DOWNCAST_VIDEO(sink);
    // only written from this thread, no need to lock
    assert(ss->video_init);

    sc_mutex_lock(&ss->mutex);

    if (ss->stopped) {
        // reject any new packet
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    if (!ss->template_ready) {
        // Init phase: buffer in the sink-level queue for sc_stream_sink_init_template.
        AVPacket *p = sc_stream_sink_packet_ref(packet);
        if (!p) {
            LOG_OOM();
            sc_mutex_unlock(&ss->mutex);
            return false;
        }
        p->stream_index = ss->video_stream.index;
        bool ok = sc_vecdeque_push(&ss->video_queue, p);
        if (!ok) {
            LOG_OOM();
            av_packet_free(&p);
            sc_mutex_unlock(&ss->mutex);
            return false;
        }
        sc_cond_signal(&ss->cond);
        sc_mutex_unlock(&ss->mutex);
        return true;
    }

    // Live phase: fan out a ref-counted copy to every active client queue.
    bool any_ok = false;
    struct sc_stream_sink_client *c = ss->clients;
    while (c) {
        if (!c->stopped) {
            AVPacket *p = sc_stream_sink_packet_ref(packet);
            if (p) {
                p->stream_index = ss->video_stream.index;
                if (sc_vecdeque_push(&c->video_queue, p)) {
                    any_ok = true;
                } else {
                    av_packet_free(&p);
                }
            }
        }
        c = c->next;
    }
    (void) any_ok; // dropping when no clients is intentional for live streams
    sc_cond_broadcast(&ss->cond);

    sc_mutex_unlock(&ss->mutex);
    return true;
}

static bool
sc_stream_sink_audio_packet_sink_open(struct sc_packet_sink *sink,
                                      AVCodecContext *ctx) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(!ss->audio_init);

    sc_mutex_lock(&ss->mutex);

    AVStream *stream = avformat_new_stream(ss->ctx, ctx->codec);
    if (!stream) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    int r = avcodec_parameters_from_context(stream->codecpar, ctx);
    if (r < 0) {
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    ss->audio_stream.index = stream->index;

    // A config packet is provided for all supported formats except raw audio
    ss->audio_expects_config_packet =
        ctx->codec_id != AV_CODEC_ID_PCM_S16LE;

    ss->audio_init = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);

    return true;
}

static void
sc_stream_sink_audio_packet_sink_close(struct sc_packet_sink *sink) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(ss->audio_init);

    sc_mutex_lock(&ss->mutex);
    // EOS also stops the stream sink
    ss->stopped = true;
    struct sc_stream_sink_client *c = ss->clients;
    while (c) {
        c->stopped = true;
        c = c->next;
    }
    sc_cond_broadcast(&ss->cond);
    sc_mutex_unlock(&ss->mutex);
}

static bool
sc_stream_sink_audio_packet_sink_push(struct sc_packet_sink *sink,
                                      const AVPacket *packet) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(ss->audio_init);

    sc_mutex_lock(&ss->mutex);

    if (ss->stopped) {
        // reject any new packet
        sc_mutex_unlock(&ss->mutex);
        return false;
    }

    if (!ss->template_ready) {
        // Init phase: buffer in the sink-level queue.
        AVPacket *p = sc_stream_sink_packet_ref(packet);
        if (!p) {
            LOG_OOM();
            sc_mutex_unlock(&ss->mutex);
            return false;
        }
        p->stream_index = ss->audio_stream.index;
        bool ok = sc_vecdeque_push(&ss->audio_queue, p);
        if (!ok) {
            LOG_OOM();
            av_packet_free(&p);
            sc_mutex_unlock(&ss->mutex);
            return false;
        }
        sc_cond_signal(&ss->cond);
        sc_mutex_unlock(&ss->mutex);
        return true;
    }

    // Live phase: fan out to every active client.
    struct sc_stream_sink_client *c = ss->clients;
    while (c) {
        if (!c->stopped) {
            AVPacket *p = sc_stream_sink_packet_ref(packet);
            if (p) {
                p->stream_index = ss->audio_stream.index;
                if (!sc_vecdeque_push(&c->audio_queue, p)) {
                    av_packet_free(&p);
                }
            }
        }
        c = c->next;
    }
    sc_cond_broadcast(&ss->cond);

    sc_mutex_unlock(&ss->mutex);
    return true;
}

static void
sc_stream_sink_audio_packet_sink_disable(struct sc_packet_sink *sink) {
    struct sc_stream_sink *ss = DOWNCAST_AUDIO(sink);
    assert(ss->audio);
    // only written from this thread, no need to lock
    assert(!ss->audio_init);

    LOGW("Audio stream disabled for stream sink");

    sc_mutex_lock(&ss->mutex);
    ss->audio = false;
    ss->audio_init = true;
    sc_cond_signal(&ss->cond);
    sc_mutex_unlock(&ss->mutex);
}

bool
sc_stream_sink_init(struct sc_stream_sink *sink, const char *url,
                    bool video, bool audio) {
    assert(video || audio);

    sink->url = strdup(url);
    if (!sink->url) {
        LOG_OOM();
        return false;
    }
    sink->video = video;
    sink->audio = audio;

    bool ok = sc_mutex_init(&sink->mutex);
    if (!ok) {
        goto error_url_free;
    }

    ok = sc_cond_init(&sink->cond);
    if (!ok) {
        goto error_mutex_destroy;
    }

    sink->stopped = false;
    sink->template_ready = false;
    sink->clients = NULL;

    sc_vecdeque_init(&sink->video_queue);
    sc_vecdeque_init(&sink->audio_queue);

    sink->video_init = false;
    sink->audio_init = false;

    sink->audio_expects_config_packet = false;

    sc_stream_sink_stream_init(&sink->video_stream);
    sc_stream_sink_stream_init(&sink->audio_stream);

    // Allocate the output format context with mpegts (for network streaming)
    const AVOutputFormat *oformat = av_guess_format("mpegts", NULL, NULL);
    if (!oformat) {
        LOGE("Could not find mpegts muxer");
        goto error_cond_destroy;
    }

    sink->ctx = avformat_alloc_context();
    if (!sink->ctx) {
        LOG_OOM();
        goto error_cond_destroy;
    }

    // Flush every packet immediately to the network: essential for live
    // streaming where any output buffering adds perceivable latency.
    // Trade-off: slightly higher CPU/network overhead per packet.
    sink->ctx->flags |= AVFMT_FLAG_FLUSH_PACKETS;
    // Limit interleave buffering so that audio and video are not held
    // waiting for each other longer than 100 ms (AV_TIME_BASE / 10).
    // Default (0) means "no limit", which causes unbounded buffering.
    sink->ctx->max_interleave_delta = AV_TIME_BASE / 10; // 100 ms

    // contrary to the deprecated API (av_oformat_next()), av_muxer_iterate()
    // returns (on purpose) a pointer-to-const, but AVFormatContext.oformat
    // still expects a pointer-to-non-const (it has not been updated accordingly)
    // <https://github.com/FFmpeg/FFmpeg/commit/0694d8702421e7aff1340038559c438b61bb30dd>
    sink->ctx->oformat = (AVOutputFormat *) oformat;

    if (video) {
        static const struct sc_packet_sink_ops video_ops = {
            .open = sc_stream_sink_video_packet_sink_open,
            .close = sc_stream_sink_video_packet_sink_close,
            .push = sc_stream_sink_video_packet_sink_push,
        };

        sink->video_packet_sink.ops = &video_ops;
    }

    if (audio) {
        static const struct sc_packet_sink_ops audio_ops = {
            .open = sc_stream_sink_audio_packet_sink_open,
            .close = sc_stream_sink_audio_packet_sink_close,
            .push = sc_stream_sink_audio_packet_sink_push,
            .disable = sc_stream_sink_audio_packet_sink_disable,
        };

        sink->audio_packet_sink.ops = &audio_ops;
    }

    return true;

error_cond_destroy:
    sc_cond_destroy(&sink->cond);
error_mutex_destroy:
    sc_mutex_destroy(&sink->mutex);
error_url_free:
    free(sink->url);

    return false;
}

bool
sc_stream_sink_start(struct sc_stream_sink *sink) {
    bool ok = sc_thread_create(&sink->thread, run_stream_sink,
                               "scrcpy-stream", sink);
    if (!ok) {
        LOGE("Could not start stream sink thread");
        return false;
    }

    return true;
}

void
sc_stream_sink_stop(struct sc_stream_sink *sink) {
    sc_mutex_lock(&sink->mutex);
    sink->stopped = true;
    // Also stop all active clients so their I/O is interrupted promptly.
    struct sc_stream_sink_client *c = sink->clients;
    while (c) {
        c->stopped = true;
        c = c->next;
    }
    sc_cond_broadcast(&sink->cond);
    sc_mutex_unlock(&sink->mutex);
}

void
sc_stream_sink_join(struct sc_stream_sink *sink) {
    sc_thread_join(&sink->thread, NULL);
}

void
sc_stream_sink_destroy(struct sc_stream_sink *sink) {
    sc_cond_destroy(&sink->cond);
    sc_mutex_destroy(&sink->mutex);
    avformat_free_context(sink->ctx);
    free(sink->url);
}
