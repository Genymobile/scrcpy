#ifndef DECODER_H
#define DECODER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

struct frames;
typedef struct SDL_Thread SDL_Thread;
typedef struct SDL_mutex SDL_mutex;

struct decoder {
    struct frames *frames;
    TCPsocket video_socket;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    SDL_bool skip_frames;
};

int decoder_start(struct decoder *decoder);
void decoder_join(struct decoder *decoder);

#endif
