#ifndef SC_CLIENT_AUDIO_H
#define SC_CLIENT_AUDIO_H

#include "common.h"

#include <stdatomic.h>
#include <stdbool.h>

#include "util/net.h"
#include "util/thread.h"

struct sc_client_audio {
    sc_socket socket;
    const char *source;
    sc_thread thread;
    atomic_bool stopped;
};

// Print the audio input sources available on the computer
void
sc_client_audio_list_sources(void);

void
sc_client_audio_init(struct sc_client_audio *ca, sc_socket socket,
                     const char *source);

bool
sc_client_audio_start(struct sc_client_audio *ca);

void
sc_client_audio_stop(struct sc_client_audio *ca);

void
sc_client_audio_join(struct sc_client_audio *ca);

#endif
