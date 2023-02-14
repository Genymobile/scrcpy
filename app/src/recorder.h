#ifndef SC_RECORDER_H
#define SC_RECORDER_H

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

#include "coords.h"
#include "options.h"
#include "trait/packet_sink.h"
#include "util/queue.h"
#include "util/thread.h"

struct sc_record_packet {
    AVPacket *packet;
    struct sc_record_packet *next;
};

struct sc_recorder_queue SC_QUEUE(struct sc_record_packet);

struct sc_recorder {
    struct sc_packet_sink packet_sink; // packet sink trait

    char *filename;
    enum sc_record_format format;
    AVFormatContext *ctx;
    struct sc_size declared_frame_size;
    bool header_written;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond queue_cond;
    // set on sc_recorder_stop(), packet_sink close or recording failure
    bool stopped;
    struct sc_recorder_queue queue;

    // wake up the recorder thread once the codec in known
    sc_cond stream_cond;
    const AVCodec *codec;

    const struct sc_recorder_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_recorder_callbacks {
    void (*on_ended)(struct sc_recorder *recorder, bool success,
                     void *userdata);
};

bool
sc_recorder_init(struct sc_recorder *recorder, const char *filename,
                 enum sc_record_format format,
                 struct sc_size declared_frame_size,
                 const struct sc_recorder_callbacks *cbs, void *cbs_userdata);

void
sc_recorder_stop(struct sc_recorder *recorder);

void
sc_recorder_join(struct sc_recorder *recorder);

void
sc_recorder_destroy(struct sc_recorder *recorder);

#endif
