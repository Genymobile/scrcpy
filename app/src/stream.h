#ifndef STREAM_H
#define STREAM_H

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>

#include "net.h"

struct video_buffer;

struct frame_meta {
    Uint64 pts;
    struct frame_meta *next;
};

struct stream {
    socket_t socket;
    struct video_buffer *video_buffer;
    SDL_Thread *thread;
    struct decoder *decoder;
    struct recorder *recorder;
    struct receiver_state {
        // meta (in order) for frames not consumed yet
        struct frame_meta *frame_meta_queue;
        size_t remaining; // remaining bytes to receive for the current frame
    } receiver_state;
};

void
stream_init(struct stream *stream, socket_t socket,
            struct decoder *decoder, struct recorder *recorder);

SDL_bool
stream_start(struct stream *stream);

void
stream_stop(struct stream *stream);

void
stream_join(struct stream *stream);

#endif
