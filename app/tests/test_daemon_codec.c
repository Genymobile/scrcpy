#include "common.h"

#include <assert.h>
#include <string.h>

#include "compat.h"
#include "daemon/codec.h"

static void
test_option_codec_names(void) {
    assert(!strcmp(sc_daemon_codec_name(SC_CODEC_H264), "h264"));
    assert(!strcmp(sc_daemon_codec_name(SC_CODEC_H265), "h265"));
    assert(!strcmp(sc_daemon_codec_name(SC_CODEC_AV1), "av1"));
    assert(!strcmp(sc_daemon_codec_name(SC_CODEC_VP8), "vp8"));
    assert(!strcmp(sc_daemon_codec_name(SC_CODEC_VP9), "vp9"));
    assert(!sc_daemon_codec_name(SC_CODEC_OPUS));
}

static void
test_avcodec_names(void) {
    assert(!strcmp(sc_daemon_avcodec_name(AV_CODEC_ID_H264), "h264"));
    assert(!strcmp(sc_daemon_avcodec_name(AV_CODEC_ID_HEVC), "h265"));
#ifdef SCRCPY_LAVC_HAS_AV1
    assert(!strcmp(sc_daemon_avcodec_name(AV_CODEC_ID_AV1), "av1"));
#endif
    assert(!strcmp(sc_daemon_avcodec_name(AV_CODEC_ID_VP8), "vp8"));
    assert(!strcmp(sc_daemon_avcodec_name(AV_CODEC_ID_VP9), "vp9"));
    assert(!sc_daemon_avcodec_name(AV_CODEC_ID_OPUS));
}

static void
test_wire_codec_ids(void) {
    assert(sc_daemon_codec_id_from_name("h264") == SC_DAEMON_CODEC_ID_H264);
    assert(sc_daemon_codec_id_from_name("h265") == SC_DAEMON_CODEC_ID_H265);
    assert(sc_daemon_codec_id_from_name("hevc") == SC_DAEMON_CODEC_ID_H265);
    assert(sc_daemon_codec_id_from_name("av1") == SC_DAEMON_CODEC_ID_AV1);
    assert(sc_daemon_codec_id_from_name("vp8") == SC_DAEMON_CODEC_ID_VP8);
    assert(sc_daemon_codec_id_from_name("vp9") == SC_DAEMON_CODEC_ID_VP9);
    assert(!sc_daemon_codec_id_from_name(NULL));
    assert(!sc_daemon_codec_id_from_name("mpeg2"));
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_option_codec_names();
    test_avcodec_names();
    test_wire_codec_ids();
    return 0;
}
