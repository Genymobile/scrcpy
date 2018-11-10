#include "recorder.h"

#include <libavutil/time.h>

#include "config.h"
#include "log.h"

static const AVOutputFormat *find_mp4_muxer(void) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 9, 100)
    void *opaque = NULL;
#endif
    const AVOutputFormat *oformat = NULL;
    do {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 9, 100)
        oformat = av_muxer_iterate(&opaque);
#else
        oformat = av_oformat_next(oformat);
#endif
        // until null or with name "mp4"
    } while (oformat && strcmp(oformat->name, "mp4"));
    return oformat;
}

SDL_bool recorder_init(struct recorder *recorder, const char *filename,
                       struct size declared_frame_size) {
    recorder->filename = SDL_strdup(filename);
    if (!recorder->filename) {
        LOGE("Cannot strdup filename");
        return SDL_FALSE;
    }

    recorder->declared_frame_size = declared_frame_size;

    return SDL_TRUE;
}

void recorder_destroy(struct recorder *recorder) {
    SDL_free(recorder->filename);
}

SDL_bool recorder_open(struct recorder *recorder, AVCodec *input_codec) {
    const AVOutputFormat *mp4 = find_mp4_muxer();
    if (!mp4) {
        LOGE("Could not find mp4 muxer");
        return SDL_FALSE;
    }

    recorder->ctx = avformat_alloc_context();
    if (!recorder->ctx) {
        LOGE("Could not allocate output context");
        return SDL_FALSE;
    }

    // contrary to the deprecated API (av_oformat_next()), av_muxer_iterate()
    // returns (on purpose) a pointer-to-const, but AVFormatContext.oformat
    // still expects a pointer-to-non-const (it has not be updated accordingly)
    // <https://github.com/FFmpeg/FFmpeg/commit/0694d8702421e7aff1340038559c438b61bb30dd>
    recorder->ctx->oformat = (AVOutputFormat *) mp4;

    AVStream *ostream = avformat_new_stream(recorder->ctx, input_codec);
    if (!ostream) {
        avformat_free_context(recorder->ctx);
        return SDL_FALSE;
    }

// In ffmpeg/doc/APIchanges:
// 2016-04-11 - 6f69f7a / 9200514 - lavf 57.33.100 / 57.5.0 - avformat.h
//   Add AVStream.codecpar, deprecate AVStream.codec.
#if    (LIBAVFORMAT_VERSION_MICRO >= 100 /* FFmpeg */ && \
        LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 33, 100)) \
    || (LIBAVFORMAT_VERSION_MICRO < 100 && /* Libav */ \
        LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 5, 0))
    ostream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    ostream->codecpar->codec_id = input_codec->id;
    ostream->codecpar->format = AV_PIX_FMT_YUV420P;
    ostream->codecpar->width = recorder->declared_frame_size.width;
    ostream->codecpar->height = recorder->declared_frame_size.height;
#else
    ostream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    ostream->codec->codec_id = input_codec->id;
    ostream->codec->pix_fmt = AV_PIX_FMT_YUV420P;
    ostream->codec->width = recorder->declared_frame_size.width;
    ostream->codec->height = recorder->declared_frame_size.height;
#endif
    ostream->time_base = (AVRational) {1, 1000000}; // timestamps in us

    int ret = avio_open(&recorder->ctx->pb, recorder->filename,
                        AVIO_FLAG_WRITE);
    if (ret < 0) {
        LOGE("Failed to open output file: %s", recorder->filename);
        // ostream will be cleaned up during context cleaning
        avformat_free_context(recorder->ctx);
        return SDL_FALSE;
    }

    ret = avformat_write_header(recorder->ctx, NULL);
    if (ret < 0) {
        LOGE("Failed to write header to %s", recorder->filename);
        avio_closep(&recorder->ctx->pb);
        avformat_free_context(recorder->ctx);
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void recorder_close(struct recorder *recorder) {
    int ret = av_write_trailer(recorder->ctx);
    if (ret < 0) {
        LOGE("Failed to write trailer to %s", recorder->filename);
    }
    avio_close(recorder->ctx->pb);
    avformat_free_context(recorder->ctx);
}

SDL_bool recorder_write(struct recorder *recorder, AVPacket *packet) {
    return av_write_frame(recorder->ctx, packet) >= 0;
}
