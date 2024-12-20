#ifndef SC_RECORDER_H
#define SC_RECORDER_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>

#include "options.h"
#include "trait/packet_sink.h"
#include "util/thread.h"
#include "util/vecdeque.h"

struct sc_recorder_queue SC_VECDEQUE(AVPacket *);

struct sc_recorder_stream {
    int index;
    int64_t last_pts;
};

struct sc_recorder {
    struct sc_packet_sink video_packet_sink;
    struct sc_packet_sink audio_packet_sink;

    /* The audio flag is unprotected:
     *  - it is initialized from sc_recorder_init() from the main thread;
     *  - it may be reset once from the recorder thread if the audio is
     *    disabled dynamically.
     *
     * Therefore, once the recorder thread is started, only the recorder thread
     * may access it without data races.
     */
    bool audio;
    bool video;

    enum sc_orientation orientation;

    char *filename;
    enum sc_record_format format;
    AVFormatContext *ctx;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    // set on sc_recorder_stop(), packet_sink close or recording failure
    bool stopped;
    struct sc_recorder_queue video_queue;
    struct sc_recorder_queue audio_queue;

    // wake up the recorder thread once the video or audio codec is known
    bool video_init;
    bool audio_init;

    bool audio_expects_config_packet;

    struct sc_recorder_stream video_stream;
    struct sc_recorder_stream audio_stream;

    const struct sc_recorder_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_recorder_callbacks {
    void (*on_ended)(struct sc_recorder *recorder, bool success,
                     void *userdata);
};

bool
sc_recorder_init(struct sc_recorder *recorder, const char *filename,
                 enum sc_record_format format, bool video, bool audio,
                 enum sc_orientation orientation,
                 const struct sc_recorder_callbacks *cbs, void *cbs_userdata);

bool
sc_recorder_start(struct sc_recorder *recorder);

void
sc_recorder_stop(struct sc_recorder *recorder);

void
sc_recorder_join(struct sc_recorder *recorder);

void
sc_recorder_destroy(struct sc_recorder *recorder);

#endif
