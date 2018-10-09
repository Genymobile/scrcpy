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
    const char *outfilename;
    struct size frame_size;
};

void decoder_init(struct decoder *decoder, struct frames *frames, socket_t video_socket, struct size frame_size);
SDL_bool decoder_start(struct decoder *decoder, const char *outfilename);
void decoder_stop(struct decoder *decoder);
void decoder_join(struct decoder *decoder);

#endif
