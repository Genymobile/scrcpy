#ifndef SC_SCREENCAP_H
#define SC_SCREENCAP_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>

#include "trait/packet_sink.h"
#include "util/thread.h"

struct sc_screencap {
    struct sc_packet_sink video_packet_sink;

    char *filename;

    AVCodecContext *codec_ctx;
    AVPacket *config_packet; // first config packet (extradata)
    bool has_config;
    bool captured;

    sc_mutex mutex;
    sc_cond cond;
    bool stopped;

    sc_thread thread;

    // The first non-config video packet to decode
    AVPacket *video_packet;
    bool video_packet_ready;

    const struct sc_screencap_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_screencap_callbacks {
    void (*on_ended)(struct sc_screencap *screencap, bool success,
                     void *userdata);
};

/**
 * Encode a decoded frame to PNG.
 *
 * On success, `*out_data` receives a malloc'd buffer (owned by the caller)
 * and `*out_size` its size.
 */
bool
sc_frame_to_png(const AVFrame *frame, uint8_t **out_data, size_t *out_size);

bool
sc_screencap_init(struct sc_screencap *screencap, const char *filename,
                  const struct sc_screencap_callbacks *cbs, void *cbs_userdata);

bool
sc_screencap_start(struct sc_screencap *screencap);

void
sc_screencap_stop(struct sc_screencap *screencap);

void
sc_screencap_join(struct sc_screencap *screencap);

void
sc_screencap_destroy(struct sc_screencap *screencap);

#endif
