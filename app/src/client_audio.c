#include "client_audio.h"
#include "util/binary.h"
#include "util/log.h"
#include "util/net.h"
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <inttypes.h>
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
        LOGE("Could not find audio input format '%s'", format_name);
        return;
    }

    LOGI("Audio input format: %s (%s)",
         input_format->name,
         input_format->long_name ? input_format->long_name : "no description");

#ifdef __APPLE__
    LOGI("Listing devices via avfoundation (see output above/below):");

    AVDictionary *options = NULL;
    av_dict_set(&options, "list_devices", "true", 0);

    AVFormatContext *fmt_ctx = NULL;
    avformat_open_input(&fmt_ctx, "", input_format, &options);

    av_dict_free(&options);
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }

    LOGI("How to use:");
    LOGI("  Use the audio device index with --client-audio-source");
    LOGI("  Format: \":AUDIO_INDEX\" (e.g., \":0\" for first audio device)");
    LOGI("Examples:");
    LOGI("  scrcpy --client-audio-source :0");
    LOGI("  scrcpy --client-audio-source file:///path/to/audio.mp3");
    return;
#endif

    LOGI("Available audio sources:");

    AVDeviceInfoList *device_list = NULL;
    int ret = avdevice_list_input_sources(input_format, NULL, NULL, &device_list);

    if (ret < 0) {
        LOGE("Could not list audio devices (error code: %d)", ret);
        LOGI("Note: You can still use device names directly with --client-audio-source");
        LOGI("Common device names:");
#ifdef _WIN32
        LOGI("  - \"audio=DEVICE_NAME\" (for dshow)");
        LOGI("  - Try running 'ffmpeg -list_devices true -f dshow -i dummy' to see available devices");
#else
        LOGI("  - \"default\" (default ALSA device)");
        LOGI("  - \"hw:0,0\" (hardware device)");
        LOGI("  - Try running 'arecord -L' to list available devices");
#endif
        return;
    }

    if (device_list->nb_devices == 0) {
        LOGI("  No audio input devices found.");
    } else {
        for (int i = 0; i < device_list->nb_devices; i++) {
            AVDeviceInfo *device = device_list->devices[i];
            if (device->device_description) {
                LOGI("  %s (%s)", device->device_name, device->device_description);
            } else {
                LOGI("  %s", device->device_name);
            }
        }
    }

    avdevice_free_list_devices(&device_list);

    LOGI("How to use:");
    LOGI("  Pass the device names shown above as --client-audio-source <device>");
    LOGI("Common microphone devices:");
#ifdef _WIN32
    LOGI("  - \"audio=DEVICE_NAME\" (use the exact name from the list)");
#else
    LOGI("  - \"default\" (usually your default microphone)");
    LOGI("  - \"hw:CARD,DEV\" devices are hardware devices");
    LOGI("  - \"pulse\" uses PulseAudio (if available)");
    LOGI("  Tip: Devices with \"capture\", \"input\", or \"microphone\" are likely input devices");
#endif
    LOGI("Examples:");
    LOGI("  scrcpy --client-audio-source default");
#ifdef _WIN32
    LOGI("  scrcpy --client-audio-source \"audio=Microphone (Realtek Audio)\"");
#else
    LOGI("  scrcpy --client-audio-source \"hw:0,0\"");
#endif
    LOGI("  scrcpy --client-audio-source file:///path/to/audio.mp3");
}

int
sc_microphone_run(void *data) {
    struct sc_microphone_params *params = (struct sc_microphone_params *)data;
    sc_socket mic_socket = params->socket;
    const char *audio_source = params->audio_source;

    int ret = 1;
    const char *input_path = NULL;
    bool is_file = parse_audio_source(audio_source, &input_path);

    AVInputFormat *input_format = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *in_codec_ctx = NULL;
    AVCodecContext *opus_ctx = NULL;
    SwrContext *swr_ctx = NULL;
    AVPacket *in_pkt = NULL;
    AVPacket *out_pkt = NULL;
    AVFrame *in_frame = NULL;
    AVFrame *out_frame = NULL;
    AVAudioFifo *fifo = NULL;

    if (is_file) {
        // Open audio file (MP3, OGG, WAV, etc.)
        LOGD("Opening audio file: %s", input_path);
        if (avformat_open_input(&fmt_ctx, input_path, NULL, NULL) < 0) {
            LOGE("Could not open audio file '%s'", input_path);
            goto cleanup;
        }
    } else {
        // Open audio device
        const char *format_name = detect_audio_format();
        LOGD("Using audio format: %s, device: %s", format_name, input_path);

        input_format = av_find_input_format(format_name);
        if (!input_format) {
            LOGE("Could not find audio input format '%s'", format_name);
            goto cleanup;
        }

        if (avformat_open_input(&fmt_ctx, input_path, input_format, NULL) < 0) {
            LOGE("Could not open audio device '%s'", input_path);
            goto cleanup;
        }
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Could not read stream info");
        goto cleanup;
    }

    int audio_stream_index = 0;
    AVCodecParameters *in_codecpar =
            fmt_ctx->streams[audio_stream_index]->codecpar;
    AVCodec *in_codec = avcodec_find_decoder(in_codecpar->codec_id);
    if (!in_codec) {
        LOGE("Input codec not found");
        goto cleanup;
    }

    in_codec_ctx = avcodec_alloc_context3(in_codec);
    if (!in_codec_ctx) {
        LOGE("Could not allocate input codec context");
        goto cleanup;
    }

    avcodec_parameters_to_context(in_codec_ctx, in_codecpar);
    if (avcodec_open2(in_codec_ctx, in_codec, NULL) < 0) {
        LOGE("Could not open input codec");
        goto cleanup;
    }

    LOGD("Input: sample_fmt=%d, sample_rate=%d, channels=%d",
       in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate,
       in_codec_ctx->ch_layout.nb_channels);

    // Setup Opus encoder
    AVCodec *opus_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!opus_codec) {
        LOGE("Opus encoder not found");
        goto cleanup;
    }

    opus_ctx = avcodec_alloc_context3(opus_codec);
    if (!opus_ctx) {
        LOGE("Could not allocate Opus codec context");
        goto cleanup;
    }

    opus_ctx->sample_fmt = AV_SAMPLE_FMT_S16; // Opus uses 16-bit PCM
    opus_ctx->sample_rate = 48000;            // Opus standard sample rate
    opus_ctx->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    opus_ctx->bit_rate = 128000; // 128 kbps

    if (avcodec_open2(opus_ctx, opus_codec, NULL) < 0) {
        LOGE("Could not open Opus encoder");
        goto cleanup;
    }
    LOGD("opus_ctx->frame_size = %d", opus_ctx->frame_size);

    LOGD("Opus encoder: sample_rate=%d, channels=%d, bit_rate=%" PRId64 " fmt=%d",
       opus_ctx->sample_rate, opus_ctx->ch_layout.nb_channels,
       opus_ctx->bit_rate, opus_ctx->sample_fmt);

    // Setup resampler (ALSA format -> Opus format)
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        LOGE("Could not allocate resampler");
        goto cleanup;
    }

    swr_alloc_set_opts2(&swr_ctx, &opus_ctx->ch_layout, opus_ctx->sample_fmt,
                                            opus_ctx->sample_rate, &in_codec_ctx->ch_layout,
                                            in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate, 0,
                                            NULL);
    if (swr_init(swr_ctx) < 0) {
        LOGE("Could not initialize resampler");
        goto cleanup;
    }

    in_pkt = av_packet_alloc();
    out_pkt = av_packet_alloc();
    in_frame = av_frame_alloc();
    out_frame = av_frame_alloc();
    if (!in_pkt || !out_pkt || !in_frame || !out_frame) {
        LOGE("Could not allocate packets/frames");
        goto cleanup;
    }

    out_frame->format = opus_ctx->sample_fmt;
    out_frame->ch_layout = opus_ctx->ch_layout;
    out_frame->sample_rate = opus_ctx->sample_rate;
    out_frame->nb_samples = opus_ctx->frame_size;
    LOGD("setting nb_samples to %d", opus_ctx->frame_size);
    if (av_frame_get_buffer(out_frame, 0) < 0) {
        LOGE("Could not allocate frame buffer");
        goto cleanup;
    }

    fifo = av_audio_fifo_alloc(opus_ctx->sample_fmt, opus_ctx->ch_layout.nb_channels,
                                                    opus_ctx->frame_size * 2);
    if (!fifo) {
        LOGE("Could not allocate audio FIFO");
        goto cleanup;
    }

    LOGI("Recording audio with Opus encoding...");
    if (is_file) {
        LOGI("File will loop continuously. Press Ctrl+C to stop.");
    }

    // For file playback: track timing to simulate real-time playback
    // Each Opus frame is opus_ctx->frame_size samples at opus_ctx->sample_rate
    int64_t start_time = -1;
    int64_t opus_frames_sent = 0;
    // Calculate frame duration in microseconds: (frame_size / sample_rate) * 1000000
    int64_t opus_frame_duration_us = (opus_ctx->frame_size * 1000000LL) / opus_ctx->sample_rate;

    // Flag to track if we should stop (e.g., socket closed)
    bool should_stop = false;

    // Main loop - for files, this will restart from beginning when reaching EOF
    while (!should_stop) {
        int read_ret = av_read_frame(fmt_ctx, in_pkt);

        // If EOF reached on a file, seek back to start and continue
        if (read_ret == AVERROR_EOF && is_file) {
            LOGD("File ended, looping back to start");
            av_seek_frame(fmt_ctx, audio_stream_index, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(in_codec_ctx);
            continue;
        }

        if (read_ret < 0) {
            // EAGAIN means no data available yet, retry
            if (read_ret == AVERROR(EAGAIN)) {
                av_usleep(1000);
                continue;
            }
            if (!is_file) {
                char errbuf[128];
                av_strerror(read_ret, errbuf, sizeof(errbuf));
                LOGD("av_read_frame error: %d (%s)", read_ret, errbuf);
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
                    ssize_t sent = net_send_all(mic_socket, size_buf, 4);
                    if (sent < 4) {
                        LOGD("Failed to send packet size, socket closed");
                        should_stop = true;
                        av_packet_unref(out_pkt);
                        break;
                    }

                    // Send Opus packet
                    sent = net_send_all(mic_socket, out_pkt->data, out_pkt->size);
                    if (sent < (ssize_t)out_pkt->size) {
                        LOGD("Failed to send packet data, socket closed");
                        should_stop = true;
                        av_packet_unref(out_pkt);
                        break;
                    }

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

                    ssize_t sent = net_send_all(mic_socket, size_buf, 4);
                    if (sent < 4) {
                        av_packet_unref(out_pkt);
                        break;  // Socket closed, skip remaining flush
                    }

                    sent = net_send_all(mic_socket, out_pkt->data, out_pkt->size);
                    if (sent < (ssize_t)out_pkt->size) {
                        av_packet_unref(out_pkt);
                        break;  // Socket closed, skip remaining flush
                    }

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

            ssize_t sent = net_send_all(mic_socket, size_buf, 4);
            if (sent < 4) {
                av_packet_unref(out_pkt);
                break;  // Socket closed, skip remaining flush
            }

            sent = net_send_all(mic_socket, out_pkt->data, out_pkt->size);
            if (sent < (ssize_t)out_pkt->size) {
                av_packet_unref(out_pkt);
                break;  // Socket closed, skip remaining flush
            }

            av_packet_unref(out_pkt);
        }

        // Give the server time to process remaining packets before closing
        av_usleep(200000); // 200ms
    }

    LOGD("Audio streaming ended");
    ret = 0;  // Success

cleanup:
    // Cleanup all resources in reverse order of allocation
    if (fifo) {
        av_audio_fifo_free(fifo);
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    if (out_frame) {
        av_frame_free(&out_frame);
    }
    if (in_frame) {
        av_frame_free(&in_frame);
    }
    if (out_pkt) {
        av_packet_free(&out_pkt);
    }
    if (in_pkt) {
        av_packet_free(&in_pkt);
    }
    if (opus_ctx) {
        avcodec_free_context(&opus_ctx);
    }
    if (in_codec_ctx) {
        avcodec_free_context(&in_codec_ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }

    // Close the socket
    if (mic_socket != SC_SOCKET_NONE) {
        if (!net_close(mic_socket)) {
            LOGW("Could not close microphone socket");
        } else {
            LOGD("Microphone socket closed");
        }
    }

    return ret;
}
