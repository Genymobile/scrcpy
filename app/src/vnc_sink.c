#include "vnc_sink.h"

#include <string.h>

#include "util/log.h"
#include "util/str.h"

/** Downcast frame_sink to sc_vnc_sink */
#define DOWNCAST(SINK) container_of(SINK, struct sc_vnc_sink, frame_sink)


/*
static int
run_vnc_sink(void *data) {
}

static bool
sc_vnc_sink_open(struct sc_vnc_sink *vs, const AVCodecContext *ctx) {
}

static void
sc_vnc_sink_close(struct sc_vnc_sink *vs) {
}

static bool
sc_vnc_sink_push(struct sc_vnc_sink *vs, const AVFrame *frame) {
}
*/

static bool
sc_vnc_frame_sink_open(struct sc_frame_sink *sink, const AVCodecContext *ctx) {
    assert(ctx->pix_fmt == AV_PIX_FMT_YUV420P);
    struct sc_vnc_sink *vnc = DOWNCAST(sink);
}

static void
sc_vnc_frame_sink_close(struct sc_frame_sink *sink) {
}

void consume_frame(struct sc_vnc_sink* vnc, const AVFrame *frame) {
	// XXX: ideally this would get "damage" regions from the decoder
	// to prevent marking the entire screen as modified if only a small
	// part changed
	if(frame->width != vnc->scrWidth || frame->height != vnc->scrHeight) {
		printf("RESIZE EVENT\n");
		if(vnc->ctx) {
			sws_freeContext(vnc->ctx);
			vnc->ctx = NULL;
		}
	}
	if(vnc->ctx == NULL) {
		vnc->scrWidth = frame->width;
		vnc->scrHeight = frame->height;
		vnc->ctx = sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P,
							  	  frame->width, frame->height, AV_PIX_FMT_RGBA,
								  0, 0, 0, 0);
		if(vnc->ctx == NULL) {
			printf("could not make context\n");
		}
		printf("good ctx\n");
		uint8_t *currentFrameBuffer = vnc->screen->frameBuffer;
		uint8_t *newFrameBuffer = (uint8_t *)malloc(vnc->scrWidth*vnc->scrHeight*vnc->bpp);
		rfbNewFramebuffer(vnc->screen, newFrameBuffer, vnc->scrWidth, vnc->scrHeight, 8, 3, vnc->bpp);
		free(currentFrameBuffer);
	}
	assert(vnc->ctx != NULL);

	int linesize[1] = {frame->width*vnc->bpp};
	int* data[1] = {vnc->screen->frameBuffer};
	sws_scale(vnc->ctx, (const uint8_t * const *)frame->data, frame->linesize, 0, frame->height, data, linesize);

	rfbMarkRectAsModified(vnc->screen,0,0,frame->width,frame->height);
}

static bool
sc_vnc_frame_sink_push(struct sc_frame_sink *sink, const AVFrame *frame) {
    struct sc_vnc_sink *vnc = DOWNCAST(sink);
    bool previous_skipped;
	consume_frame(vnc, frame);
	return true;
}

bool
sc_vnc_sink_init(struct sc_vnc_sink *vs, const char *device_name) {
    bool ok = sc_frame_buffer_init(&vs->fb);
    if (!ok) {
        return false;
    }
    static const struct sc_frame_sink_ops ops = {
        .open = sc_vnc_frame_sink_open,
        .close = sc_vnc_frame_sink_close,
        .push = sc_vnc_frame_sink_push,
    };

    vs->frame_sink.ops = &ops;
	vs->bpp = 4;
	vs->screen = rfbGetScreen(0,NULL,32,32,8,3,vs->bpp);
	vs->screen->frameBuffer = (uint8_t*)malloc(32*32*vs->bpp);
	rfbInitServer(vs->screen);
	rfbRunEventLoop(vs->screen,-1,TRUE);
	return true;
}

void
sc_vnc_sink_destroy(struct sc_vnc_sink *vs) {
	if(vs->screen) {
		free(vs->screen->frameBuffer);
		rfbScreenCleanup(vs->screen);
	}
	if(vs->ctx) {
		sws_freeContext(vs->ctx);
	}
}
