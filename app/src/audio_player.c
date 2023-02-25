#include "audio_player.h"

#include <libavutil/opt.h>

#include "util/log.h"

/** Downcast frame_sink to sc_v4l2_sink */
#define DOWNCAST(SINK) container_of(SINK, struct sc_audio_player, frame_sink)

#define SC_AV_SAMPLE_FMT AV_SAMPLE_FMT_S16
#define SC_SDL_SAMPLE_FMT AUDIO_S16

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

static size_t
sc_audio_player_get_swr_buf_size(struct sc_audio_player *ap, size_t samples) {
    assert(ap->nb_channels);
    assert(ap->out_bytes_per_sample);
    return samples * ap->nb_channels * ap->out_bytes_per_sample;
}

static uint8_t *
sc_audio_player_get_swr_buf(struct sc_audio_player *ap, size_t min_samples) {
    size_t min_buf_size = sc_audio_player_get_swr_buf_size(ap, min_samples);
    if (min_buf_size < ap->swr_buf_alloc_size) {
        size_t new_size = min_buf_size + 4096;
        uint8_t *buf = realloc(ap->swr_buf, new_size);
        if (!buf) {
            LOG_OOM();
            // Could not realloc to the requested size
            return NULL;
        }
        ap->swr_buf = buf;
        ap->swr_buf_alloc_size = new_size;
    }

    return ap->swr_buf;
}

static bool
sc_audio_player_frame_sink_open(struct sc_frame_sink *sink,
                                const AVCodecContext *ctx) {
    struct sc_audio_player *ap = DOWNCAST(sink);

    SwrContext *swr_ctx = ap->swr_ctx;
    assert(swr_ctx);

    assert(ctx->sample_rate > 0);
    assert(ctx->ch_layout.nb_channels > 0);
    assert(!av_sample_fmt_is_planar(SC_AV_SAMPLE_FMT));
    int out_bytes_per_sample = av_get_bytes_per_sample(SC_AV_SAMPLE_FMT);
    assert(out_bytes_per_sample > 0);

    av_opt_set_chlayout(swr_ctx, "in_chlayout", &ctx->ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", ctx->sample_fmt, 0);

    av_opt_set_chlayout(swr_ctx, "out_chlayout", &ctx->ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", SC_AV_SAMPLE_FMT, 0);

    int ret = swr_init(swr_ctx);
    if (ret) {
        LOGE("Failed to initialize the resampling context");
        return false;
    }

    ap->sample_rate = ctx->sample_rate;
    ap->nb_channels = ctx->ch_layout.nb_channels;
    ap->out_bytes_per_sample = out_bytes_per_sample;

    size_t initial_swr_buf_size = sc_audio_player_get_swr_buf_size(ap, 4096);
    ap->swr_buf = malloc(initial_swr_buf_size);
    if (!ap->swr_buf) {
        LOG_OOM();
        return false;
    }

    SDL_AudioSpec desired = {
        .freq = ctx->sample_rate,
        .format = SC_SDL_SAMPLE_FMT,
        .channels = ctx->ch_layout.nb_channels,
        .samples = 512, // ~10ms at 48000Hz
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

    SwrContext *swr_ctx = ap->swr_ctx;

    int64_t delay = swr_get_delay(swr_ctx, ap->sample_rate);
    // No need to av_rescale_rnd(), input and output sample rates are the same
    int dst_nb_samples = delay + frame->nb_samples;

    uint8_t *swr_buf = sc_audio_player_get_swr_buf(ap, frame->nb_samples);
    if (!swr_buf) {
        return false;
    }

    int ret = swr_convert(swr_ctx, &swr_buf, dst_nb_samples,
                          (const uint8_t **) frame->data, frame->nb_samples);
    if (ret < 0) {
        LOGE("Resampling failed: %d", ret);
        return false;
    }
    LOGI("ret=%d dst_nb_samples=%d\n", ret, dst_nb_samples);

    size_t swr_buf_size = sc_audio_player_get_swr_buf_size(ap, ret);
    LOGI("== swr_buf_size %lu", swr_buf_size);

    // TODO clock drift compensation

    // It should almost always be possible to write without lock
    bool can_write_without_lock = swr_buf_size <= ap->safe_empty_buffer;
    if (can_write_without_lock) {
        sc_bytebuf_prepare_write(&ap->buf, swr_buf, swr_buf_size);
    }

    SDL_LockAudioDevice(ap->device);
    if (can_write_without_lock) {
        sc_bytebuf_commit_write(&ap->buf, swr_buf_size);
    } else {
        sc_bytebuf_write(&ap->buf, swr_buf, swr_buf_size);
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

    ap->swr_ctx = swr_alloc();
    if (!ap->swr_ctx) {
        sc_bytebuf_destroy(&ap->buf);
        LOG_OOM();
        return false;
    }

    ap->safe_empty_buffer = sc_bytebuf_write_remaining(&ap->buf);

    ap->swr_buf = NULL;
    ap->swr_buf_alloc_size = 0;

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
    swr_free(&ap->swr_ctx);
    free(ap->swr_buf);
}
