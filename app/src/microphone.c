#include "microphone.h"
#include "util/binary.h"
#include "util/log.h"
#include "util/net.h"
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Detect platform-specific audio input format
static const char *detect_audio_format(void) {
#ifdef _WIN32
    // Try WASAPI first (more modern), fallback to dshow
    if (av_find_input_format("wasapi")) {
        return "wasapi";
    }
    return "dshow";
#elif defined(__APPLE__)
    return "avfoundation";
#else
    // Linux and others
    return "alsa";
#endif
}

// Parse audio source to determine if it's a file or device
// Returns true if it's a file, false if it's a device
static bool parse_audio_source(const char *source, const char **path) {
    if (strncmp(source, "file://", 7) == 0) {
        *path = source + 7;  // Skip "file://"
        return true;
    }
    *path = source;
    return false;
}

// List available audio input sources on the client machine
void
sc_microphone_list_audio_sources(void) {
    const char *format_name = detect_audio_format();
    AVInputFormat *input_format = av_find_input_format(format_name);

    if (!input_format) {
        fprintf(stderr, "Could not find audio input format '%s'\n", format_name);
        return;
    }

    printf("Audio input format: %s (%s)\n",
         input_format->name,
         input_format->long_name ? input_format->long_name : "no description");
    printf("\nAvailable audio sources:\n");

    AVDeviceInfoList *device_list = NULL;
    int ret = avdevice_list_input_sources(input_format, NULL, NULL, &device_list);

    if (ret < 0) {
        fprintf(stderr, "Could not list audio devices (error code: %d)\n", ret);
        fprintf(stderr, "Note: You can still use device names directly with --client-audio-source\n");
        fprintf(stderr, "Common device names:\n");
#ifdef _WIN32
        fprintf(stderr, "  - \"audio=DEVICE_NAME\" (for dshow)\n");
        fprintf(stderr, "  - Try running 'ffmpeg -list_devices true -f dshow -i dummy' to see available devices\n");
#elif defined(__APPLE__)
        fprintf(stderr, "  - \":0\" (default microphone)\n");
        fprintf(stderr, "  - Try running 'ffmpeg -f avfoundation -list_devices true -i \"\"' to see available devices\n");
#else
        fprintf(stderr, "  - \"default\" (default ALSA device)\n");
        fprintf(stderr, "  - \"hw:0,0\" (hardware device)\n");
        fprintf(stderr, "  - Try running 'arecord -L' to list available devices\n");
#endif
        return;
    }

    if (device_list->nb_devices == 0) {
        printf("  No audio input devices found.\n");
    } else {
        for (int i = 0; i < device_list->nb_devices; i++) {
            AVDeviceInfo *device = device_list->devices[i];
            printf("  %s", device->device_name);
            if (device->device_description) {
                printf(" (%s)", device->device_description);
            }
            printf("\n");
        }
    }

    avdevice_free_list_devices(&device_list);

    printf("\nHow to use:\n");
    printf("  Use the device name exactly as shown above with --client-audio-source\n");
    printf("\nCommon microphone devices:\n");
#ifdef _WIN32
    printf("  - \"audio=DEVICE_NAME\" (use the exact name from the list)\n");
#elif defined(__APPLE__)
    printf("  - \":0\" or \":1\" (device indices for macOS)\n");
    printf("  - \"default\" (default microphone)\n");
#else
    printf("  - \"default\" (usually your default microphone)\n");
    printf("  - \"hw:CARD,DEV\" devices are hardware devices\n");
    printf("  - \"pulse\" uses PulseAudio (if available)\n");
    printf("  Tip: Devices with \"capture\", \"input\", or \"microphone\" are likely input devices\n");
#endif
    printf("\nExamples:\n");
    printf("  scrcpy --client-audio-source default\n");
#ifdef _WIN32
    printf("  scrcpy --client-audio-source \"audio=Microphone (Realtek Audio)\"\n");
#else
    printf("  scrcpy --client-audio-source \"hw:0,0\"\n");
#endif
    printf("  scrcpy --client-audio-source file:///path/to/audio.mp3\n");
}

int
sc_microphone_run(void *data) {
    struct sc_microphone_params *params = (struct sc_microphone_params *)data;
    sc_socket mic_socket = params->socket;
    const char *audio_source = params->audio_source;

    const char *input_path = NULL;
    bool is_file = parse_audio_source(audio_source, &input_path);

    AVInputFormat *input_format = NULL;
    AVFormatContext *fmt_ctx = NULL;

    if (is_file) {
        // Open audio file (MP3, OGG, WAV, etc.)
        LOGD("Opening audio file: %s", input_path);
        if (avformat_open_input(&fmt_ctx, input_path, NULL, NULL) < 0) {
            fprintf(stderr, "Could not open audio file '%s'\n", input_path);
            net_close(mic_socket);
            return 1;
        }
    } else {
        // Open audio device
        const char *format_name = detect_audio_format();
        LOGD("Using audio format: %s, device: %s", format_name, input_path);

        input_format = av_find_input_format(format_name);
        if (!input_format) {
            fprintf(stderr, "Could not find audio input format '%s'\n", format_name);
            net_close(mic_socket);
            return 1;
        }

        if (avformat_open_input(&fmt_ctx, input_path, input_format, NULL) < 0) {
            fprintf(stderr, "Could not open audio device '%s'\n", input_path);
            net_close(mic_socket);
            return 1;
        }
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not read stream info\n");
        avformat_close_input(&fmt_ctx);
        net_close(mic_socket);
        return 1;
    }

    int audio_stream_index = 0;
    AVCodecParameters *in_codecpar =
            fmt_ctx->streams[audio_stream_index]->codecpar;
    AVCodec *in_codec = avcodec_find_decoder(in_codecpar->codec_id);
    if (!in_codec) {
        fprintf(stderr, "Input codec not found\n");
        avformat_close_input(&fmt_ctx);
        net_close(mic_socket);
        return 1;
    }

    AVCodecContext *in_codec_ctx = avcodec_alloc_context3(in_codec);
    avcodec_parameters_to_context(in_codec_ctx, in_codecpar);
    if (avcodec_open2(in_codec_ctx, in_codec, NULL) < 0) {
        fprintf(stderr, "Could not open input codec\n");
        avcodec_free_context(&in_codec_ctx);
        avformat_close_input(&fmt_ctx);
        net_close(mic_socket);
        return 1;
    }

    LOGD("Input: sample_fmt=%d, sample_rate=%d, channels=%d\n",
       in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate,
       in_codec_ctx->ch_layout.nb_channels);

    // Setup Opus encoder
    AVCodec *opus_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!opus_codec) {
        fprintf(stderr, "Opus encoder not found\n");
        avcodec_free_context(&in_codec_ctx);
        avformat_close_input(&fmt_ctx);
        net_close(mic_socket);
        return 1;
    }

    AVCodecContext *opus_ctx = avcodec_alloc_context3(opus_codec);
    opus_ctx->sample_fmt = AV_SAMPLE_FMT_S16; // Opus uses 16-bit PCM
    opus_ctx->sample_rate = 48000;            // Opus standard sample rate
    opus_ctx->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    opus_ctx->bit_rate = 128000; // 128 kbps

    if (avcodec_open2(opus_ctx, opus_codec, NULL) < 0) {
        fprintf(stderr, "Could not open Opus encoder\n");
        avcodec_free_context(&opus_ctx);
        avcodec_free_context(&in_codec_ctx);
        avformat_close_input(&fmt_ctx);
        net_close(mic_socket);
        return 1;
    }
    LOGD("opus_ctx->frame_size = %d\n", opus_ctx->frame_size);

    LOGD("Opus encoder: sample_rate=%d, channels=%d, bit_rate=%d fmt=%d\n",
       opus_ctx->sample_rate, opus_ctx->ch_layout.nb_channels,
       opus_ctx->bit_rate, opus_ctx->sample_fmt);

    // Setup resampler (ALSA format -> Opus format)
    SwrContext *swr_ctx = swr_alloc();
    swr_alloc_set_opts2(&swr_ctx, &opus_ctx->ch_layout, opus_ctx->sample_fmt,
                                            opus_ctx->sample_rate, &in_codec_ctx->ch_layout,
                                            in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate, 0,
                                            NULL);
    swr_init(swr_ctx);

    AVPacket *in_pkt = av_packet_alloc();
    AVPacket *out_pkt = av_packet_alloc();
    AVFrame *in_frame = av_frame_alloc();
    AVFrame *out_frame = av_frame_alloc();

    out_frame->format = opus_ctx->sample_fmt;
    out_frame->ch_layout = opus_ctx->ch_layout;
    out_frame->sample_rate = opus_ctx->sample_rate;
    out_frame->nb_samples = opus_ctx->frame_size;
    LOGD("setting nb_samples to %d\n", opus_ctx->frame_size);
    av_frame_get_buffer(out_frame, 0);

    AVAudioFifo *fifo =
            av_audio_fifo_alloc(opus_ctx->sample_fmt, opus_ctx->ch_layout.nb_channels,
                                                    opus_ctx->frame_size * 2);

    printf("Recording audio with Opus encoding...\n");
    if (is_file) {
        printf("File will loop continuously. Press Ctrl+C to stop.\n");
    }

    // For file playback: track timing to simulate real-time playback
    // Each Opus frame is opus_ctx->frame_size samples at opus_ctx->sample_rate
    int64_t start_time = -1;
    int64_t opus_frames_sent = 0;
    // Calculate frame duration in microseconds: (frame_size / sample_rate) * 1000000
    int64_t opus_frame_duration_us = (opus_ctx->frame_size * 1000000LL) / opus_ctx->sample_rate;

    // Main loop - for files, this will restart from beginning when reaching EOF
    while (true) {
        int ret = av_read_frame(fmt_ctx, in_pkt);

        // If EOF reached on a file, seek back to start and continue
        if (ret == AVERROR_EOF && is_file) {
            LOGD("File ended, looping back to start");
            av_seek_frame(fmt_ctx, audio_stream_index, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(in_codec_ctx);
            continue;
        }

        if (ret < 0) {
            // For device input, any error means we should stop
            if (!is_file) {
                break;
            }
            continue;
        }
        if (avcodec_send_packet(in_codec_ctx, in_pkt) < 0)
            continue;

        while (avcodec_receive_frame(in_codec_ctx, in_frame) >= 0) {
            // Resample to Opus format
            int out_samples =
                    swr_convert(swr_ctx, (uint8_t **)out_frame->data, opus_ctx->frame_size,
                                            (const uint8_t **)in_frame->data, in_frame->nb_samples);

            if (out_samples <= 0)
                continue;

            av_audio_fifo_write(fifo, (void **)out_frame->data, out_samples);

            while (av_audio_fifo_size(fifo) >= opus_ctx->frame_size) {
                av_audio_fifo_read(fifo, (void **)out_frame->data,
                           opus_ctx->frame_size);

                out_frame->nb_samples = opus_ctx->frame_size;
                out_frame->pts = av_rescale_q(in_frame->pts, in_codec_ctx->time_base,
                                                                            opus_ctx->time_base);

                // Encode to Opus
                if (avcodec_send_frame(opus_ctx, out_frame) < 0)
                    continue;

                while (avcodec_receive_packet(opus_ctx, out_pkt) >= 0) {
                    uint32_t size = out_pkt->size;
                    uint8_t size_buf[4];
                    sc_write32be(size_buf, size);

                    // Send packet size (4 bytes, big-endian)
                    net_send_all(mic_socket, size_buf, 4);

                    // Send Opus packet
                    net_send_all(mic_socket, out_pkt->data, out_pkt->size);

                    av_packet_unref(out_pkt);

                    // For file input, add timing control AFTER sending
                    if (is_file) {
                        if (start_time < 0) {
                            start_time = av_gettime_relative();
                        }

                        opus_frames_sent++;

                        // Calculate expected time based on Opus frames sent
                        int64_t expected_time_us = opus_frames_sent * opus_frame_duration_us;
                        int64_t elapsed_time_us = av_gettime_relative() - start_time;
                        int64_t sleep_time_us = expected_time_us - elapsed_time_us;

                        if (sleep_time_us > 0) {
                            av_usleep(sleep_time_us);
                        }
                    }
                }
            }
        }
        av_packet_unref(in_pkt);
    }

    // Only flush and cleanup for device input
    // For file input, the loop is infinite (controlled by user/scrcpy termination)
    if (!is_file) {
        // Flush the decoder
        avcodec_send_packet(in_codec_ctx, NULL);
        while (avcodec_receive_frame(in_codec_ctx, in_frame) >= 0) {
            int out_samples =
                    swr_convert(swr_ctx, (uint8_t **)out_frame->data, opus_ctx->frame_size,
                                            (const uint8_t **)in_frame->data, in_frame->nb_samples);
            if (out_samples > 0) {
                av_audio_fifo_write(fifo, (void **)out_frame->data, out_samples);
            }
        }

        // Flush any remaining audio in the FIFO
        while (av_audio_fifo_size(fifo) > 0) {
            int samples_to_read = av_audio_fifo_size(fifo);
            if (samples_to_read > opus_ctx->frame_size) {
                samples_to_read = opus_ctx->frame_size;
            }

            av_audio_fifo_read(fifo, (void **)out_frame->data, samples_to_read);
            out_frame->nb_samples = samples_to_read;

            if (avcodec_send_frame(opus_ctx, out_frame) >= 0) {
                while (avcodec_receive_packet(opus_ctx, out_pkt) >= 0) {
                    uint32_t size = out_pkt->size;
                    uint8_t size_buf[4];
                    sc_write32be(size_buf, size);

                    net_send_all(mic_socket, size_buf, 4);
                    net_send_all(mic_socket, out_pkt->data, out_pkt->size);

                    av_packet_unref(out_pkt);
                }
            }
        }

        // Flush the Opus encoder
        avcodec_send_frame(opus_ctx, NULL);
        while (avcodec_receive_packet(opus_ctx, out_pkt) >= 0) {
            uint32_t size = out_pkt->size;
            uint8_t size_buf[4];
            sc_write32be(size_buf, size);

            net_send_all(mic_socket, size_buf, 4);
            net_send_all(mic_socket, out_pkt->data, out_pkt->size);

            av_packet_unref(out_pkt);
        }

        // Give the server time to process remaining packets before closing
        av_usleep(200000); // 200ms
    }

    LOGD("Audio streaming ended");

    // Cleanup
    av_audio_fifo_free(fifo);
    swr_free(&swr_ctx);
    av_frame_free(&out_frame);
    av_frame_free(&in_frame);
    av_packet_free(&out_pkt);
    av_packet_free(&in_pkt);
    avcodec_free_context(&opus_ctx);
    avcodec_free_context(&in_codec_ctx);
    avformat_close_input(&fmt_ctx);

    // Close the socket
    net_close(mic_socket);
    LOGD("Microphone socket closed");

    return 0;
}
