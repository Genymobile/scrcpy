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
    bool stopped; // set on recorder_close()
    bool failed; // set on packet write failure
    struct sc_recorder_queue queue;

    // we can write a packet only once we received the next one so that we can
    // set its duration (next_pts - current_pts)
    // "previous" is only accessed from the recorder thread, so it does not
    // need to be protected by the mutex
    struct sc_record_packet *previous;
};

bool
sc_recorder_init(struct sc_recorder *recorder, const char *filename,
                 enum sc_record_format format,
                 struct sc_size declared_frame_size);

void
sc_recorder_destroy(struct sc_recorder *recorder);

#endif
