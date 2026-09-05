#include "common.h"

#include <assert.h>
#include <string.h>

#include "daemon/clip_buffer.h"

static const struct sc_clip_entry ENTRIES[] = {
    {.pts = 0,    .key = true},
    {.pts = 1000, .key = false},
    {.pts = 2000, .key = false},
    {.pts = 3000, .key = true},
    {.pts = 4000, .key = false},
};

static void
test_end_is_exclusive(void) {
    size_t begin;
    size_t stop;

    bool ok = sc_clip_select(ENTRIES, ARRAY_LEN(ENTRIES), 0, 3000,
                             &begin, &stop);
    assert(ok);
    assert(begin == 0);
    assert(stop == 2);

    ok = sc_clip_select(ENTRIES, ARRAY_LEN(ENTRIES), 0, 1, &begin, &stop);
    assert(ok);
    assert(begin == 0);
    assert(stop == 0);

    ok = sc_clip_select(ENTRIES, ARRAY_LEN(ENTRIES), 0, 0, &begin, &stop);
    assert(!ok);
}

static void
test_start_snaps_to_keyframe(void) {
    size_t begin;
    size_t stop;

    bool ok = sc_clip_select(ENTRIES, ARRAY_LEN(ENTRIES), 2500, 4500,
                             &begin, &stop);
    assert(ok);
    assert(begin == 0);
    assert(stop == 4);

    ok = sc_clip_select(ENTRIES, ARRAY_LEN(ENTRIES), 3000, 4500,
                        &begin, &stop);
    assert(ok);
    assert(begin == 3);
    assert(stop == 4);
}

static void
test_packet_durations_hold_exact_end(void) {
    assert(sc_clip_packet_duration_us(ENTRIES, 3, 0, 10000) == 1000);
    assert(sc_clip_packet_duration_us(ENTRIES, 3, 1, 10000) == 1000);
    assert(sc_clip_packet_duration_us(ENTRIES, 3, 2, 10000) == 8000);
}

static void
test_decoded_frame_origin_is_exact(void) {
    static const struct sc_clip_entry entries[] = {
        {.pts = 1000, .key = true},  // decoder preroll
        {.pts = 2000, .key = true},  // first retained decoded frame
        {.pts = 3000, .key = false},
    };
    size_t index;
    assert(sc_clip_find_timeline_origin(entries, ARRAY_LEN(entries), 2000,
                                        &index) == 0);
    assert(index == 1);

    // The origin may never be rounded or snapped to a neighboring packet.
    assert(sc_clip_find_timeline_origin(entries, ARRAY_LEN(entries), 2500,
                                        &index) == SC_CLIP_EINTERNAL);
    assert(sc_clip_find_timeline_origin(entries, ARRAY_LEN(entries), 4000,
                                        &index) == SC_CLIP_ERANGE);

    static const struct sc_clip_entry non_key_origin[] = {
        {.pts = 1000, .key = true},
        {.pts = 2000, .key = false},
    };
    assert(sc_clip_find_timeline_origin(non_key_origin,
                                        ARRAY_LEN(non_key_origin), 2000,
                                        &index) == SC_CLIP_EINTERNAL);
}

static void
test_epoch_boundaries_are_exact(void) {
    static const struct sc_clip_entry entries[] = {
        {.pts = 0,    .epoch = 0, .key = true},
        {.pts = 1000, .epoch = 0, .key = false},
        {.pts = 2000, .epoch = 1, .key = true},
        {.pts = 3000, .epoch = 1, .key = false},
        {.pts = 4000, .epoch = 2, .key = true},
    };
    size_t begin;
    size_t stop;
    int64_t boundary;

    int r = sc_clip_select_epoch(entries, ARRAY_LEN(entries), 0, 2000,
                                 &begin, &stop, &boundary);
    assert(!r);
    assert(begin == 0);
    assert(stop == 1);

    r = sc_clip_select_epoch(entries, ARRAY_LEN(entries), 2000, 4000,
                             &begin, &stop, &boundary);
    assert(!r);
    assert(begin == 2);
    assert(stop == 3);

    r = sc_clip_select_epoch(entries, ARRAY_LEN(entries), 1000, 3000,
                             &begin, &stop, &boundary);
    assert(r == SC_CLIP_ESESSION);
    assert(boundary == 2000);

    r = sc_clip_select_epoch(entries, ARRAY_LEN(entries), 2000, 5000,
                             &begin, &stop, &boundary);
    assert(r == SC_CLIP_ESESSION);
    assert(boundary == 4000);

    static const struct sc_clip_entry fractional_boundary[] = {
        {.pts = 0,    .epoch = 0, .key = true},
        {.pts = 1000, .epoch = 0, .key = false},
        {.pts = 2500, .epoch = 1, .key = true},
        {.pts = 3500, .epoch = 1, .key = false},
    };
    // The API accepts milliseconds, so the reported 2ms boundary must also be
    // a usable start even when the first packet is timestamped at 2.5ms.
    r = sc_clip_select_epoch(fractional_boundary,
                             ARRAY_LEN(fractional_boundary), 2000, 4000,
                             &begin, &stop, &boundary);
    assert(!r);
    assert(begin == 2);
    assert(stop == 3);
}

static void
test_codec_compatible_container(void) {
    const struct sc_clip_format *format;

    format = sc_clip_format_for_codec(AV_CODEC_ID_H264);
    assert(format);
    assert(!strcmp(format->container, "mp4"));
    assert(!strcmp(format->extension, ".mp4"));

    format = sc_clip_format_for_codec(AV_CODEC_ID_VP9);
    assert(format);
    assert(!strcmp(format->container, "mp4"));

    format = sc_clip_format_for_codec(AV_CODEC_ID_VP8);
    assert(format);
    assert(!strcmp(format->container, "webm"));
    assert(!strcmp(format->extension, ".webm"));
    assert(!strcmp(format->muxer_name, "webm"));

    assert(!sc_clip_format_for_codec(AV_CODEC_ID_OPUS));
}

static void
test_epoch_compatibility_is_exact(void) {
    struct sc_clip_epoch a = {
        .par = avcodec_parameters_alloc(),
    };
    struct sc_clip_epoch b = {
        .par = avcodec_parameters_alloc(),
    };
    assert(a.par);
    assert(b.par);

    a.par->codec_type = b.par->codec_type = AVMEDIA_TYPE_VIDEO;
    a.par->codec_id = b.par->codec_id = AV_CODEC_ID_H264;
    a.par->format = b.par->format = AV_PIX_FMT_YUV420P;
    a.par->width = b.par->width = 1080;
    a.par->height = b.par->height = 1920;
    assert(sc_clip_epochs_compatible(&a, &b));

    b.par->width = 720;
    assert(!sc_clip_epochs_compatible(&a, &b));
    b.par->width = a.par->width;

    static uint8_t config_a[] = {1, 2, 3};
    static uint8_t config_b[] = {1, 2, 3};
    a.config = config_a;
    a.config_size = sizeof(config_a);
    b.config = config_b;
    b.config_size = sizeof(config_b);
    assert(sc_clip_epochs_compatible(&a, &b));

    config_b[2] = 4;
    assert(!sc_clip_epochs_compatible(&a, &b));

    avcodec_parameters_free(&a.par);
    avcodec_parameters_free(&b.par);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_end_is_exclusive();
    test_start_snaps_to_keyframe();
    test_packet_durations_hold_exact_end();
    test_decoded_frame_origin_is_exact();
    test_epoch_boundaries_are_exact();
    test_codec_compatible_container();
    test_epoch_compatibility_is_exact();
    return 0;
}
