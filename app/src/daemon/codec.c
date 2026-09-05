#include "codec.h"

#include <string.h>

#include "compat.h"

const char *
sc_daemon_codec_name(enum sc_codec codec) {
    switch (codec) {
        case SC_CODEC_H264:
            return "h264";
        case SC_CODEC_H265:
            return "h265";
        case SC_CODEC_AV1:
            return "av1";
        case SC_CODEC_VP8:
            return "vp8";
        case SC_CODEC_VP9:
            return "vp9";
        default:
            return NULL;
    }
}

const char *
sc_daemon_avcodec_name(enum AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_H264:
            return "h264";
        case AV_CODEC_ID_HEVC:
            return "h265";
#ifdef SCRCPY_LAVC_HAS_AV1
        case AV_CODEC_ID_AV1:
            return "av1";
#endif
        case AV_CODEC_ID_VP8:
            return "vp8";
        case AV_CODEC_ID_VP9:
            return "vp9";
        default:
            return NULL;
    }
}

uint32_t
sc_daemon_codec_id_from_name(const char *codec) {
    if (!codec) {
        return 0;
    }
    if (!strcmp(codec, "h264")) {
        return SC_DAEMON_CODEC_ID_H264;
    }
    if (!strcmp(codec, "h265") || !strcmp(codec, "hevc")) {
        return SC_DAEMON_CODEC_ID_H265;
    }
    if (!strcmp(codec, "av1")) {
        return SC_DAEMON_CODEC_ID_AV1;
    }
    if (!strcmp(codec, "vp8")) {
        return SC_DAEMON_CODEC_ID_VP8;
    }
    if (!strcmp(codec, "vp9")) {
        return SC_DAEMON_CODEC_ID_VP9;
    }
    return 0;
}
