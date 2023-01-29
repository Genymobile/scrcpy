#include "audio_player.h"

#include <SDL2/SDL.h>
#include <assert.h>

#include "util/log.h"

static void callback(void *userdata, unsigned char *buf, int len) {
    struct sc_audio_player *audio_player =
        SDL_static_cast(struct sc_audio_player *, userdata);

    int head = 0;
    for (; head < len;) {
        ssize_t r =
            net_recv(audio_player->audio_socket, buf + head, len - head);
        if (r <= 0) {
            SDL_PauseAudioDevice(audio_player->dev, 1);
            return;
        }
        head += (int)r;
    }
}

bool sc_audio_player_init(struct sc_audio_player *audio_player,
                          sc_socket audio_socket) {
    SDL_AudioSpec want, have;
    SDL_zero(want);

    want.freq = 48000;
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = 4096;
    want.callback = callback;
    want.userdata = audio_player;

    if (SDL_Init(SDL_INIT_AUDIO)) {
        LOGE("SDL_Init(SDL_INIT_AUDIO) failed: %s", SDL_GetError());
        return false;
    }

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev <= 0) {
        LOGE("SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    audio_player->audio_socket = audio_socket;
    audio_player->dev = dev;
    return true;
}

void sc_audio_player_start(struct sc_audio_player *audio_player) {
    assert(audio_player->audio_socket);
    assert(audio_player->dev);

    SDL_PauseAudioDevice(audio_player->dev, 0);
}

void sc_audio_player_destory(struct sc_audio_player *audio_player) {
    SDL_CloseAudioDevice(audio_player->dev);
}
