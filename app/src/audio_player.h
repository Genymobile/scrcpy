#ifndef SC_AUDIO_PLAYER_H
#define SC_AUDIO_PLAYER_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "audio_regulator.h"
#include "trait/frame_sink.h"
#include "util/tick.h"

struct sc_audio_player {
    struct sc_frame_sink frame_sink;

    // The target buffering between the producer and the consumer. This value
    // is directly use for compensation.
    // Since audio capture and/or encoding on the device typically produce
    // blocks of 960 samples (20ms) or 1024 samples (~21.3ms), this target
    // value should be higher.
    sc_tick target_buffering_delay;

    // SDL audio output buffer size
    sc_tick output_buffer_duration;

    SDL_AudioDeviceID device;
    struct sc_audio_regulator audioreg;
};

void
sc_audio_player_init(struct sc_audio_player *ap, sc_tick target_buffering,
                     sc_tick audio_output_buffer);

#endif
