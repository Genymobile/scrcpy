#ifndef SC_DAEMON_CODEC_H
#define SC_DAEMON_CODEC_H

#include "common.h"

#include <stdint.h>

#include <libavcodec/codec_id.h>

#include "options.h"

// Codec ids used by the scrcpy device protocol (4-byte ASCII names).
#define SC_DAEMON_CODEC_ID_H264 UINT32_C(0x68323634)
#define SC_DAEMON_CODEC_ID_H265 UINT32_C(0x68323635)
#define SC_DAEMON_CODEC_ID_AV1  UINT32_C(0x00617631)
#define SC_DAEMON_CODEC_ID_VP8  UINT32_C(0x00767038)
#define SC_DAEMON_CODEC_ID_VP9  UINT32_C(0x00767039)

/**
 * Return the canonical daemon protocol name for a video codec.
 *
 * These helpers deliberately return NULL for non-video/unknown codecs instead
 * of silently pretending they are H.264. A wrong codec label makes downstream
 * decoders fail in ways which are much harder to diagnose than an explicit
 * unsupported-codec error.
 */
const char *
sc_daemon_codec_name(enum sc_codec codec);

const char *
sc_daemon_avcodec_name(enum AVCodecID codec_id);

/**
 * Convert a daemon protocol codec name to the scrcpy device wire id.
 *
 * "hevc" is accepted as a compatibility alias for canonical "h265".
 * Returns 0 for an unknown name.
 */
uint32_t
sc_daemon_codec_id_from_name(const char *codec);

#endif
