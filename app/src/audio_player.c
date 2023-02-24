#include "audio_player.h"

#include "util/log.h"

/** Downcast frame_sink to sc_v4l2_sink */
#define DOWNCAST(SINK) container_of(SINK, struct sc_audio_player, frame_sink)

void
sc_audio_player_sdl_callback(void *userdata, uint8_t *stream, int len_int) {
    struct sc_audio_player *ap = userdata;

    // This callback is called with the lock used by SDL_AudioDeviceLock(), so
    // the bytebuf is protected

    assert(len_int > 0);
    size_t len = len_int;

    size_t read = sc_bytebuf_read_remaining(&ap->buf);
    if (read) {
        if (read > len) {
            read = len;
        }
        sc_bytebuf_read(&ap->buf, stream, read);
    }
    if (read < len) {
        // Insert silence
        memset(stream + read, 0, len - read);
    }
}

static SDL_AudioFormat
sc_audio_player_ffmpeg_to_sdl_format(enum AVSampleFormat format) {
    switch (format) {
        case AV_SAMPLE_FMT_S16:
            return AUDIO_S16;
        case AV_SAMPLE_FMT_S32:
            return AUDIO_S32;
        case AV_SAMPLE_FMT_FLT:
            return AUDIO_F32;
        default:
            LOGE("Unsupported FFmpeg sample format: %s",
                 av_get_sample_fmt_name(format));
            return 0;
    }
}

static bool
sc_audio_player_frame_sink_open(struct sc_frame_sink *sink,
                                const AVCodecContext *ctx) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    SDL_AudioFormat format =
        sc_audio_player_ffmpeg_to_sdl_format(ctx->sample_fmt);
    if (!format) {
        // error already logged
        //return false;
        format = AUDIO_F32; // it's planar, but for now there is only 1 channel
    }
    LOGI("%d\n", ctx->sample_rate);

    SDL_AudioSpec desired = {
        .freq = ctx->sample_rate,
        .format = format,
        .channels = ctx->ch_layout.nb_channels,
        .samples = 2048,
        .callback = sc_audio_player_sdl_callback,
        .userdata = ap,
    };
    SDL_AudioSpec obtained;

    ap->device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (!ap->device) {
        LOGE("Could not open audio device: %s", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(ap->device, 0);

    return true;
}

static void
sc_audio_player_frame_sink_close(struct sc_frame_sink *sink) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    assert(ap->device);
    SDL_PauseAudioDevice(ap->device, 1);
    SDL_CloseAudioDevice(ap->device);
}

static bool
sc_audio_player_frame_sink_push(struct sc_frame_sink *sink, const AVFrame *frame) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    const uint8_t *data = frame->data[0];
    size_t size = frame->linesize[0];

    // TODO convert to non planar format
    // TODO then re-enable stereo
    // TODO clock drift compensation

    // It should almost always be possible to write without lock
    bool can_write_without_lock = size <= ap->safe_empty_buffer;
    if (can_write_without_lock) {
        sc_bytebuf_prepare_write(&ap->buf, data, size);
    }

    SDL_LockAudioDevice(ap->device);
    if (can_write_without_lock) {
        sc_bytebuf_commit_write(&ap->buf, size);
    } else {
        sc_bytebuf_write(&ap->buf, data, size);
    }

    // The next time, it will remain at least the current empty space
    ap->safe_empty_buffer = sc_bytebuf_write_remaining(&ap->buf);
    SDL_UnlockAudioDevice(ap->device);

    return true;
}

bool
sc_audio_player_init(struct sc_audio_player *ap,
                     const struct sc_audio_player_callbacks *cbs,
                     void *cbs_userdata) {
    bool ok = sc_bytebuf_init(&ap->buf, 128 * 1024);
    if (!ok) {
        return false;
    }

    ap->safe_empty_buffer = sc_bytebuf_write_remaining(&ap->buf);

    assert(cbs && cbs->on_ended);
    ap->cbs = cbs;
    ap->cbs_userdata = cbs_userdata;

    static const struct sc_frame_sink_ops ops = {
        .open = sc_audio_player_frame_sink_open,
        .close = sc_audio_player_frame_sink_close,
        .push = sc_audio_player_frame_sink_push,
    };

    ap->frame_sink.ops = &ops;
    return true;
}

void
sc_audio_player_destroy(struct sc_audio_player *ap) {
    sc_bytebuf_destroy(&ap->buf);
}
