#ifndef SC_CLIENT_AUDIO_H
#define SC_CLIENT_AUDIO_H

#include "util/net.h"

struct sc_microphone_params {
    sc_socket socket;
    const char *audio_source;
};

void
sc_microphone_list_audio_sources(void);

int
sc_microphone_run(void *data);

#endif
