#ifndef SC_VNC_SINK_H
#define SC_VNC_SINK_H

#include "common.h"

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <rfb/rfb.h>

#include "coords.h"
#include "trait/frame_sink.h"
#include "frame_buffer.h"
#include "util/tick.h"

struct sc_vnc_sink {
    struct sc_frame_sink frame_sink; // frame sink trait

    struct sc_frame_buffer fb;
    AVFormatContext *format_ctx;
    AVCodecContext *encoder_ctx;
	struct SwsContext * ctx;
	rfbScreenInfoPtr screen;
	uint8_t frameBuffer;
	uint16_t scrWidth;
	uint16_t scrHeight;
	uint8_t bpp;

    char *device_name;

    sc_thread thread;
    sc_mutex mutex;
    sc_cond cond;
    bool has_frame;
    bool stopped;
    bool header_written;

    AVFrame *frame;
    AVPacket *packet;
};

bool
sc_vnc_sink_init(struct sc_vnc_sink *vs, const char *device_name);

void
sc_vnc_sink_destroy(struct sc_vnc_sink *vs);

#endif
