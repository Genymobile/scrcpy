#ifndef RECORDER_H
#define RECORDER_H

#include "common.h"

#include <stdbool.h>
#include <libavformat/avformat.h>

#include "coords.h"
#include "options.h"
#include "trait/packet_sink.h"
#include "util/queue.h"
#include "util/thread.h"

struct record_packet {
    AVPacket *packet;
    struct record_packet *next;
};

struct recorder_queue SC_QUEUE(struct record_packet);

struct recorder {
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
    struct recorder_queue queue;

    // we can write a packet only once we received the next one so that we can
    // set its duration (next_pts - current_pts)
    // "previous" is only accessed from the recorder thread, so it does not
    // need to be protected by the mutex
    struct record_packet *previous;
};

bool
recorder_init(struct recorder *recorder, const char *filename,
              enum sc_record_format format, struct sc_size declared_frame_size);

void
recorder_destroy(struct recorder *recorder);

#endif
