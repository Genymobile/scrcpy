#ifndef DECODER_H
#define DECODER_H

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>

#include "common.h"
#include "net.h"

struct frames;

struct decoder {
    struct frames *frames;
    socket_t video_socket;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    struct recorder *recorder;
    struct receiver_state {
        uint64_t next_pts;
        uint64_t pts;
        size_t remaining; // remaining bytes to receive for the current frame
    } receiver_state;
};

void decoder_init(struct decoder *decoder, struct frames *frames,
                  socket_t video_socket, struct recorder *recoder);
SDL_bool decoder_start(struct decoder *decoder);
void decoder_stop(struct decoder *decoder);
void decoder_join(struct decoder *decoder);

#endif
