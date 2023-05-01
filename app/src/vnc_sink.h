#ifndef SC_VNC_SINK_H
#define SC_VNC_SINK_H

#include "common.h"

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <rfb/rfb.h>

#include "coords.h"
#include "control_msg.h"
#include "controller.h"
#include "trait/frame_sink.h"
#include "frame_buffer.h"
#include "util/tick.h"

struct sc_vnc_sink {
    struct sc_frame_sink frame_sink; // frame sink trait
    struct sc_controller *controller;

    struct SwsContext * ctx;
    rfbScreenInfoPtr screen;
    uint16_t scrWidth;
    uint16_t scrHeight;
    uint8_t bpp;

    bool was_down;
    char *device_name;
};

bool
sc_vnc_sink_init(struct sc_vnc_sink *vs, const char *device_name, struct sc_controller *controller);

void
sc_vnc_sink_destroy(struct sc_vnc_sink *vs);

void
ptr_add_event(int buttonMask, int x, int y, rfbClientPtr cl);
#endif
