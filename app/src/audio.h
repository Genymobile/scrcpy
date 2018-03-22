#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_stdinc.h>

struct audio_player {
    const char *serial;
    SDL_AudioDeviceID input_device;
    SDL_AudioDeviceID output_device;
};

SDL_bool sdl_audio_init(void);

// serial must not be NULL
SDL_bool audio_player_init(struct audio_player *player, const char *serial);
void audio_player_destroy(struct audio_player *player);

SDL_bool audio_player_open(struct audio_player *player);
void audio_player_close(struct audio_player *player);

void audio_player_play(struct audio_player *player);
void audio_player_pause(struct audio_player *player);

// for convenience, these functions handle everything
SDL_bool audio_forwarding_start(struct audio_player *player, const char *serial);
void audio_forwarding_stop(struct audio_player *player);

#endif
