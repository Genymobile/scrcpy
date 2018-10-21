#ifndef AOA_H
#define AOA_H

#include <SDL2/SDL_stdinc.h>

#define AUDIO_MODE_NO_AUDIO               0
#define AUDIO_MODE_S16LSB_STEREO_44100HZ  1

SDL_bool aoa_init(void);
void aoa_exit(void);

// serial must not be NULL
SDL_bool aoa_forward_audio(const char *serial, SDL_bool forward);

#endif
