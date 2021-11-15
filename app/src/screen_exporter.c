#include <stdio.h>
#include "screen_exporter.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define DOWNCAST(SINK) container_of(SINK, struct screen_exporter, frame_sink)

 int avframe_to_jpeg_packet(AVPacket* packet, AVFrame *pFrame) {
    AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec) {
        printf("failed to get codec\n");
        return -1;
    }
    AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegContext) {
        printf("failed to get context\n");
        return -1;
    }
    jpegContext->pix_fmt = AV_PIX_FMT_YUVJ420P;
    jpegContext->height = pFrame->height;
    jpegContext->width = pFrame->width;
    jpegContext->time_base= (AVRational){1,25};
    int code = avcodec_open2(jpegContext, jpegCodec, NULL);
    if (code < 0) {
        printf("openning jpeg failed %d %d\n", code, errno);
        return -1;
    }
    
    int gotFrame;

    if (avcodec_encode_video2(jpegContext, packet, pFrame, &gotFrame) < 0) {
        printf("encoding failed\n");
        return -1;
    }

    avcodec_close(jpegContext);
    return 1;
}

const size_t BYTES_PER_INT = sizeof(int); 

int intToCharArray(char* buffer, int in)
{
	for (size_t i = 0; i < BYTES_PER_INT; i++) {
		size_t shift = 8 * (BYTES_PER_INT - 1 - i);
		buffer[i] = (in >> shift) & 0xff;
	}
	return 0;
}

static bool
frame_sink_open(struct sc_frame_sink *sink, const AVCodec *codec) {
    struct screen_exporter *screen_exporter = DOWNCAST(sink);
    return true;
}

static void
frame_sink_close(struct sc_frame_sink *sink) {
    struct screen_exporter *screen_exporter = DOWNCAST(sink);
}

static bool
frame_sink_push(struct sc_frame_sink *sink, const AVFrame *frame) {
    struct screen_exporter *screen_exporter = DOWNCAST(sink);
    char hello[256];
    int sock = screen_exporter->sock;

    AVPacket packet = {.data = NULL, .size = 0};
    av_init_packet(&packet);
    if (avframe_to_jpeg_packet(&packet, frame) < 0) 
    {
        printf("failed to transcoding to jpeg!");
        return false;
    }
    
    char buffer[BYTES_PER_INT];
    intToCharArray(buffer, packet.size);
    if (send(sock , buffer , BYTES_PER_INT , 0 ) < 0) 
    {
        return false;
    }
    if (send(sock , packet.data , packet.size , 0 ) < 0) 
    {
        return false;
    }
    
    return true;
}

void
screen_exporter_init(struct screen_exporter *screen_exporter) {
    int sock = 0;
	struct sockaddr_in serv_addr;
	char buffer[1024] = {0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int rtn = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (rtn < 0)
	{
		printf("\nConnection Failed %d\n", rtn);
		return;
	}
	
    screen_exporter->sock = sock;
    
    static const struct sc_frame_sink_ops ops = {
        .open = frame_sink_open,
        .close = frame_sink_close,
        .push = frame_sink_push,
    };

    screen_exporter->frame_sink.ops = &ops;
}