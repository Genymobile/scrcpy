#ifndef RECORDER_H
#define RECORDER_H

#include <stdbool.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>

#include "config.h"
#include "common.h"
#include "util/queue.h"

enum recorder_format {
    RECORDER_FORMAT_AUTO,
    RECORDER_FORMAT_MP4,
    RECORDER_FORMAT_MKV,
};

struct record_packet {
    AVPacket packet;
    struct record_packet *next;
};

struct recorder_queue QUEUE(struct record_packet);

struct recorder {
    char *filename;
    enum recorder_format format;
    AVFormatContext *ctx;
    struct size declared_frame_size;
    bool header_written;

    SDL_Thread *thread;
    SDL_mutex *mutex;
    SDL_cond *queue_cond;
    bool stopped; // set on recorder_stop() by the stream reader
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
              enum recorder_format format, struct size declared_frame_size);

void
recorder_destroy(struct recorder *recorder);

bool
recorder_open(struct recorder *recorder, const AVCodec *input_codec);

void
recorder_close(struct recorder *recorder);

bool
recorder_start(struct recorder *recorder);

void
recorder_stop(struct recorder *recorder);

void
recorder_join(struct recorder *recorder);

bool
recorder_push(struct recorder *recorder, const AVPacket *packet);

#endif
