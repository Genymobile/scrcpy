#ifndef SC_AUDIO_PLAYER_H
#define SC_AUDIO_PLAYER_H

#include "common.h"

#include <SDL2/SDL_audio.h>
#include <stdbool.h>

#include "util/net.h"
#include "util/thread.h"

struct sc_audio_player {
  sc_socket audio_socket;
  SDL_AudioDeviceID dev;
  sc_thread thread;
};

bool sc_audio_player_init(struct sc_audio_player *audio_player,
                          sc_socket audio_socket);

void sc_audio_player_start(struct sc_audio_player *audio_player);

void sc_audio_player_destory(struct sc_audio_player *audio_player);

#endif
