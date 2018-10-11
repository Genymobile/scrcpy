#ifndef DECODER_H
#define DECODER_H

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>

#include "common.h"
#include "net.h"

struct frames;

struct decoder {
    uint64_t pts;
    struct frames *frames;
    socket_t video_socket;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    const char *out_filename;
    struct size frame_size;
    int remaining;
};

void decoder_init(struct decoder *decoder, struct frames *frames, socket_t video_socket, struct size frame_size);
SDL_bool decoder_start(struct decoder *decoder, const char *out_filename);
void decoder_stop(struct decoder *decoder);
void decoder_join(struct decoder *decoder);

#endif
