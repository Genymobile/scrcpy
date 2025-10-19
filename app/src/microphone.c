#include "microphone.h"
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include "util/log.h"
#include "util/net.h"

int read_mic(void *data) {
    sc_socket mic_socket = *(sc_socket*)data;
    const char *input_format_name = "alsa";
    const char *device_name = "default";

    AVInputFormat *input_format = av_find_input_format(input_format_name);
    if (!input_format) {
        fprintf(stderr, "Could not find ALSA input format\n");
        return 1;
    }

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, device_name, input_format, NULL) < 0) {
        fprintf(stderr, "Could not open ALSA device '%s'\n", device_name);
        return 1;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not read stream info\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    // ALSA has only one audio stream
    int audio_stream_index = 0;
    AVCodecParameters *codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return 1;
    }
		LOGD("sample_fmt = %d\n", codec_ctx->sample_fmt);
		LOGD("sample rate = %d\n", codec_ctx->sample_rate);

    AVPacket pkt;
    AVFrame *frame = av_frame_alloc();
    printf("Recording audio (Ctrl+C to stop)...\n");

    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        // ALSA always gives audio packets, so skip the stream_index check
        if (avcodec_send_packet(codec_ctx, &pkt) >= 0) {
            while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                int bytes_per_sample = av_get_bytes_per_sample(codec_ctx->sample_fmt);
                int data_size = frame->nb_samples * frame->ch_layout.nb_channels * bytes_per_sample;

                net_send_all(mic_socket, frame->data[0], data_size);
            }
        }
        av_packet_unref(&pkt);
    }

    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}
